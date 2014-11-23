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
}

MOESIF_protocol::~MOESIF_protocol ()
{    
}

void MOESIF_protocol::dump (void)
{
    const char *block_states[9] = {"X","I","S","E","O","M","F"};
    fprintf (stderr, "MOESIF_protocol - state: %s\n", block_states[state]);
}

void MOESIF_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
    case MOESIF_CACHE_I:  do_cache_I (request); break;
    case MOESIF_CACHE_S: do_cache_S (request); break;
    case MOESIF_CACHE_E:  do_cache_M (request); break;
    case MOESIF_CACHE_O:  do_cache_O (request); break;
    case MOESIF_CACHE_M:  do_cache_M (request); break;
    case MOESIF_CACHE_F:  do_cache_F (request); break;
    default:
      fatal_error ("MOESIF_protocol->state not valid?\n");
    }
}

void MOESIF_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
    case MOESIF_CACHE_I:  do_snoop_I (request); break;
    case MOESIF_CACHE_S: do_snoop_S (request); break;
    case MOESIF_CACHE_E:  do_snoop_M (request); break;
    case MOESIF_CACHE_O:  do_snoop_O (request); break;
    case MOESIF_CACHE_M:  do_snoop_M (request); break;
    case MOESIF_CACHE_F:  do_snoop_F (request); break;
    default:
    	fatal_error ("MOESIF_protocol->state not valid?\n");
    }
}

inline void MOESIF_protocol::do_cache_F (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: F state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_I (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: I state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_S (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: S state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_E (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: E state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_O (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: O state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_cache_M (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: M state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_F (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: F state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_I (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: I state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_S (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: S state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_E (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: E state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_O (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: O state shouldn't see this message\n");
  }
}

inline void MOESIF_protocol::do_snoop_M (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: M state shouldn't see this message\n");
  }
}



