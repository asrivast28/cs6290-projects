#include "MOESIF_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MOESIF_protocol::MOESIF_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
  this->state = MOESIF_CACHE_I;
}

MOESIF_protocol::~MOESIF_protocol ()
{    
}

void MOESIF_protocol::dump (void)
{
    const char *block_states[10] = {"X","I","IS","S","E","F","O","IM","OM","M"};
    fprintf (stderr, "MOESIF_protocol - state: %s\n", block_states[state]);
}

void MOESIF_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
    case MOESIF_CACHE_I:  do_cache_I (request); break;
    case MOESIF_CACHE_IS: do_cache_IS_IM (request); break;
    case MOESIF_CACHE_S: do_cache_S (request); break;
    case MOESIF_CACHE_E:  do_cache_E (request); break;
    case MOESIF_CACHE_F:  do_cache_F (request); break;
    case MOESIF_CACHE_O:  do_cache_O (request); break;
    case MOESIF_CACHE_IM: do_cache_IS_IM (request); break;
    case MOESIF_CACHE_OM: do_cache_IS_IM (request); break;
    case MOESIF_CACHE_M:  do_cache_M (request); break;
    default:
      fatal_error ("MOESIF_protocol->state not valid?\n");
    }
}

void MOESIF_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
    case MOESIF_CACHE_I:  do_snoop_I (request); break;
    case MOESIF_CACHE_IS: do_snoop_IS (request); break;
    case MOESIF_CACHE_S: do_snoop_S (request); break;
    case MOESIF_CACHE_E:  do_snoop_E (request); break;
    case MOESIF_CACHE_F:  do_snoop_F (request); break;
    case MOESIF_CACHE_O:  do_snoop_O (request); break;
    case MOESIF_CACHE_IM: do_snoop_IM (request); break;
    case MOESIF_CACHE_OM: do_snoop_OM (request); break;
    case MOESIF_CACHE_M:  do_snoop_M (request); break;
    default:
    	fatal_error ("MOESIF_protocol->state not valid?\n");
    }
}

inline void MOESIF_protocol::do_cache_I (Mreq *request)
{
  switch (request->msg) {
    // If we get a request from the processor we need to get the data
    case LOAD:
      // Line up the GETS in the Bus' queue
      send_GETS(request->addr);
      // Set the state to IS
      state = MOESIF_CACHE_IS;
      // This is a read miss
      Sim->cache_misses++;
      break;
    case STORE:
      // Line up the GETM in the Bus' queue
      send_GETM(request->addr);
      // Set the state to IM
      state = MOESIF_CACHE_IM;
      // This is a write miss
      Sim->cache_misses++;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: I state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_IS_IM (Mreq *request)
{
	switch (request->msg) {
    case LOAD:
    case STORE:
      /**
       * If the block is in either IS or IM state, that means it sent out a GET message
       * and is waiting on DATA.  Therefore the processor should be waiting
       * on a pending request. Therefore we should not be getting any requests from
       * the processor.
       */
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error("Should only have one outstanding request per processor!");
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: IS or IM state shouldn't see this message\n");
	}
}

inline void MOESIF_protocol::do_cache_S (Mreq *request)
{
  switch (request->msg) {
    case LOAD:
      // Send data back to the processor to finish the request
      send_DATA_to_proc(request->addr);
      break;
    case STORE:
      // Line up the GETM in the Bus' queue
      send_GETM(request->addr);
      // Set the state to IM
      state = MOESIF_CACHE_IM;
      // This is also a write miss
      Sim->cache_misses++;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: S state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_E (Mreq *request)
{
  switch (request->msg) {
    case LOAD:
      // Send data back to the processor to finish the request
      send_DATA_to_proc(request->addr);
      break;
    case STORE:
      // Send data back to the processor to finish the request
      send_DATA_to_proc(request->addr);
      // Silently upgrade to M
      state = MOESIF_CACHE_M;
      Sim->silent_upgrades++;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: E state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_F (Mreq *request)
{
  switch (request->msg) {
    case LOAD:
      // Send data back to the processor to finish the request
      send_DATA_to_proc(request->addr);
    case STORE:
      send_GETM(request->addr);
      state = MOESIF_CACHE_IM;
      Sim->cache_misses++;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: F state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_O (Mreq *request)
{
  switch (request->msg) {
    case LOAD:
      send_DATA_to_proc(request->addr);
      break;
    case STORE:
      send_GETM(request->addr);
      state = MOESIF_CACHE_OM;
      Sim->cache_misses++;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: O state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_M (Mreq *request)
{
  switch (request->msg) {
    case LOAD:
    case STORE:
      // Send data back to the processor to finish the request
      send_DATA_to_proc(request->addr);
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: M state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_I (Mreq *request)
{
  switch (request->msg) {
    case GETS:
    case GETM:
    case DATA:
    	/**
    	 * If we snoop a message from another cache and we are in I, then we don't
    	 * need to do anything!  We obviously cannot supply data since we don't have
    	 * it, and we don't need to downgrade our state since we are already in I.
    	 */
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: I state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_IS (Mreq *request)
{
  switch (request->msg) {
    case GETS:
    case GETM:
      /**
       * While in IS we will see our own GETS or GETM on the bus. We should just
       * ignore it and wait for DATA to show up.
       */
      break;
    case DATA:
      /**
       * IS state meant that the block had sent GETS and was waiting on DATA.
       * Now that Data is received we can send the DATA to the processor and finish
       * the transition to S or E.
       */
      send_DATA_to_proc(request->addr);
      if (get_shared_line()) {
        state = MOESIF_CACHE_S;
      }
      else {
        state = MOESIF_CACHE_E;
      }
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: IS state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_S (Mreq *request)
{
  switch (request->msg) {
    case GETS:
      set_shared_line();
      break;
    case GETM:
      state = MOESIF_CACHE_I;
      break;
    case DATA:
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: S state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_E (Mreq *request)
{
  switch (request->msg) {
    case GETS:
      set_shared_line();
      send_DATA_on_bus(request->addr, request->src_mid);
      state = MOESIF_CACHE_F;
      break;
    case GETM:
      set_shared_line();
      send_DATA_on_bus(request->addr, request->src_mid);
      state = MOESIF_CACHE_I;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: E state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_F (Mreq *request)
{
  switch (request->msg) {
    case GETS:
      set_shared_line();
      send_DATA_on_bus(request->addr, request->src_mid);
      break;
    case GETM:
      set_shared_line();
      send_DATA_on_bus(request->addr, request->src_mid);
      state = MOESIF_CACHE_I;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: F state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_O (Mreq *request)
{
  switch (request->msg) {
    case GETS:
      set_shared_line();
      send_DATA_on_bus(request->addr, request->src_mid);
      break;
    case GETM:
      set_shared_line();
      send_DATA_on_bus(request->addr, request->src_mid);
      state = MOESIF_CACHE_I;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: O state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_IM (Mreq *request)
{
	switch (request->msg) {
    case GETS:
    case GETM:
      /**
       * While in IM we will see our own GETS or GETM on the bus. We should just
       * ignore it and wait for DATA to show up.
       */
      break;
    case DATA:
      /**
       * IM state meant that the block had sent GETM and was waiting on DATA.
       * Now that Data is received we can send the DATA to the processor and finish
       * the transition to M.
       */
      send_DATA_to_proc(request->addr);
      state = MOESIF_CACHE_M;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: IM state shouldn't see this message\n");
	}
}

inline void MOESIF_protocol::do_snoop_OM (Mreq *request)
{
	switch (request->msg) {
    case GETS:
    case GETM:
      if (!get_shared_line()) {
        set_shared_line();
        send_DATA_on_bus(request->addr, request->src_mid);
      }
      break;
    case DATA:
      /**
       * IM state meant that the block had sent GETM and was waiting on DATA.
       * Now that Data is received we can send the DATA to the processor and finish
       * the transition to M.
       */
      send_DATA_to_proc(request->addr);
      state = MOESIF_CACHE_M;
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: IM state shouldn't see this message\n");
	}
}

inline void MOESIF_protocol::do_snoop_M (Mreq *request)
{
  switch (request->msg) {
    case GETS:
    	/**
    	 * Another cache wants to share data so we send it to them and transition to
    	 * S. When we send the DATA it will go on the bus the next cycle and the memory
       * will see it and cancel its lookup for the DATA.
    	 */
      set_shared_line();
      send_DATA_on_bus(request->addr, request->src_mid);
      state = MOESIF_CACHE_O;
      break;
    case GETM:
    	/**
    	 * Another cache wants the data so we send it to them and transition to
    	 * I since they will be transitioning to M.  When we send the DATA
    	 * it will go on the bus the next cycle and the memory will see it and cancel
    	 * its lookup for the DATA.
    	 */
      set_shared_line();
      send_DATA_on_bus(request->addr, request->src_mid);
      state = MOESIF_CACHE_I;
      break;
    case DATA:
      fatal_error ("Should not see data for this line!  I have the line!\n");
      break;
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: M state shouldn't see this message\n");
  }
}
