#include "MESI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MESI_protocol::MESI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
}

MESI_protocol::~MESI_protocol ()
{    
}

void MESI_protocol::dump (void)
{
    const char *block_states[5] = {"X","I","S","E","M"};
    fprintf (stderr, "MESI_protocol - state: %s\n", block_states[state]);
}

void MESI_protocol::process_cache_request (Mreq *request)
{
	switch (state) {
    case MESI_CACHE_I:  do_cache_I (request); break;
    case MESI_CACHE_S: do_cache_S (request); break;
    case MESI_CACHE_E:  do_cache_M (request); break;
    case MESI_CACHE_M:  do_cache_M (request); break;
    default:
      fatal_error ("MESI_protocol->state not valid?\n");
    }
}

void MESI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
    case MESI_CACHE_I:  do_snoop_I (request); break;
    case MESI_CACHE_S: do_snoop_S (request); break;
    case MESI_CACHE_E:  do_snoop_M (request); break;
    case MESI_CACHE_M:  do_snoop_M (request); break;
    default:
      fatal_error ("MESI_protocol->state not valid?\n");
    }
}

inline void MESI_protocol::do_cache_I (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: I state shouldn't see this message\n");
  }
}

inline void MESI_protocol::do_cache_S (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: S state shouldn't see this message\n");
  }
}

inline void MESI_protocol::do_cache_E (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: E state shouldn't see this message\n");
  }
}

inline void MESI_protocol::do_cache_M (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: M state shouldn't see this message\n");
  }
}

inline void MESI_protocol::do_snoop_I (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: I state shouldn't see this message\n");
  }
}

inline void MESI_protocol::do_snoop_S (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: S state shouldn't see this message\n");
  }
}

inline void MESI_protocol::do_snoop_E (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: E state shouldn't see this message\n");
  }
}

inline void MESI_protocol::do_snoop_M (Mreq *request)
{
  switch (request->msg) {
    default:
      request->print_msg (my_table->moduleID, "ERROR");
      fatal_error ("Client: M state shouldn't see this message\n");
  }
}

