#include "procsim.hpp"

#include "tomasulo.hpp"

#include <cinttypes>

// Object of Tomasulo Simulator class.
TomasuloSimulator ts;

/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 *
 * @r ROB size
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f) 
{
  uint64_t k[] = {k0, k1, k2};
  ts = TomasuloSimulator(r, k, f);
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats)
{
  ts.simulateProcessor(p_stats);
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) 
{
  ts.printInstructionCycles();
  double cycle_count_double = static_cast<double>(p_stats->cycle_count);
  p_stats->retired_instruction = ts.retiredInstruction();
  p_stats->avg_inst_retired = ts.retiredInstruction() / cycle_count_double; 
  p_stats->avg_inst_fired = ts.firedInstruction() / cycle_count_double; 
  p_stats->avg_disp_size = ts.dispatchQueueSize() / cycle_count_double;
}
