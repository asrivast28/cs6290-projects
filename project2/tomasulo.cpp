#include "tomasulo.hpp"

#include <algorithm>
#include <iostream>

// set this to 1 for printing out what happens in each cycle to stderr
#define DEBUG_LOG 0

/**
 * @brief Default constructor for the simulator class.
 */
TomasuloSimulator::TomasuloSimulator(
) : m_schedulingQueue(),
  m_instructionCycleLog(0),
  m_waitingInstructions(0),
  m_resultBuses(0),
  m_scoreboard(),
  m_regFile(),
  m_dispatchQueue(),
  m_schedulingQueueCapacity(0),
  m_fetchRate(0),
  m_reservedSlots(0),
  m_dispatchQueueSize(0),
  m_firedInstruction(0),
  m_retiredInstruction(0),
  m_counter(0),
  m_doneFetching(true)
{
}

/**
 * @brief Constructor for constructing 
 *
 * @param r   Number of result buses to be used.
 * @param k   Array which specifies number of functional units, of each type, to be used.
 * @param f   Number of instructions to fetch in each cycle. 
 */
TomasuloSimulator::TomasuloSimulator(
  const uint64_t r,
  const uint64_t k[NUM_FU_TYPES],
  const uint64_t f
) : m_schedulingQueue(),
  m_instructionCycleLog(0),
  m_waitingInstructions(0),
  m_resultBuses(r),
  m_scoreboard(),
  m_regFile(),
  m_dispatchQueue(),
  m_schedulingQueueCapacity(0),
  m_fetchRate(f),
  m_reservedSlots(0),
  m_dispatchQueueSize(0),
  m_firedInstruction(0),
  m_counter(0),
  m_doneFetching(false)
{
  for (uint64_t i = 0; i < NUM_FU_TYPES; ++i) {
    // scheduling capacity is twice the number of function units
    m_schedulingQueueCapacity += 2 * k[i];
    for (uint64_t j = 0; j < k[i]; ++j) {
      // initialize all the scoreboard slots to -1
      m_scoreboard[i].push_back(-1);
    }
  }

  for (uint64_t i = 0; i < NUM_REGISTERS; ++i) {
    // all the registers are ready initially
    m_regFile[i].first = true;
  }
}

/**
 * @brief Copy constructor. 
 *
 * @param ts  The instance to be copied from.
 */
TomasuloSimulator::TomasuloSimulator(
  const TomasuloSimulator& ts
)
{
  // just copy all the values from the other instance
  m_schedulingQueue = ts.m_schedulingQueue;
  m_instructionCycleLog = ts.m_instructionCycleLog;
  m_waitingInstructions = ts.m_waitingInstructions;
  m_resultBuses = ts.m_resultBuses;
  m_scoreboard = ts.m_scoreboard;
  m_regFile = ts.m_regFile;
  m_dispatchQueue = ts.m_dispatchQueue;
  m_schedulingQueueCapacity = ts.m_schedulingQueueCapacity,
  m_fetchRate = ts.m_fetchRate;
  m_reservedSlots = ts.m_reservedSlots;
  m_dispatchQueueSize = ts.m_dispatchQueueSize;
  m_firedInstruction = ts.m_firedInstruction;
  m_retiredInstruction = ts.m_retiredInstruction;
  m_counter = ts.m_counter;
  m_doneFetching = ts.m_doneFetching;

  for (uint64_t i = 0; i < NUM_FU_TYPES; ++i) {
    m_scoreboard[i] = ts.m_scoreboard[i];
  }
  for (uint64_t i = 0; i < NUM_REGISTERS; ++i) {
    m_regFile[i] = ts.m_regFile[i];
  }
}

/**
 * @brief Function which fetches instructions.
 *
 * @param p_stats     Pointer to the statistics structure. 
 * @param firstHalf   Variable for indicating if it is the first half of the cycle.
 */
void
TomasuloSimulator::fetch(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  if (!firstHalf) {
    for (uint64_t f = 0; f < m_fetchRate; ++f) {
      proc_inst_t p_inst;
      if (read_instruction(&p_inst)) {
        // create a new entry in the cycle log for the fetched instruction
        m_instructionCycleLog.push_back(std::array<unsigned long, NUM_STAGES>());
        // initialize logs with 0
        (m_instructionCycleLog.rbegin())->fill(0);
        // assign tag to be line number of the instruction
        p_inst.tag = m_counter++;
        // push the instruction to dispatch queue 
        m_dispatchQueue.push(p_inst);
        // set instruction fetch cycle to current cycle 
        m_instructionCycleLog[p_inst.tag][0] = p_stats->cycle_count;
        // set instruction dispatch cycle to next cycle 
        m_instructionCycleLog[p_inst.tag][1] = (p_stats->cycle_count + 1);
#if DEBUG_LOG
        std::cerr << p_stats->cycle_count << "\tFETCHED\t" << (p_inst.tag + 1) << std::endl;
#endif
      }
      else {
        // no more instructions to fetch
        m_doneFetching = true;
        break;
      }
    }

    // add to dispatch queue size for calculating average size later
    m_dispatchQueueSize += m_dispatchQueue.size(); 
    // update maximum dispatch queue size
    if (m_dispatchQueue.size() > p_stats->max_disp_size) {
      p_stats->max_disp_size = static_cast<unsigned long>(m_dispatchQueue.size());
    }
  }
}

/**
 * @brief Function which dispatches instructions.
 *
 * @param p_stats     Pointer to the statistics structure. 
 * @param firstHalf   Variable for indicating if it is the first half of the cycle.
 */
void
TomasuloSimulator::dispatch(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  if (firstHalf) {
    // reserve slots in the scheduling queue during first half cycle
    m_reservedSlots = std::min(m_schedulingQueueCapacity - m_schedulingQueue.size(), m_dispatchQueue.size());
  }
  else {
    // push the instructions in scheduling queue in the second half cycle
    while ((m_reservedSlots > 0) && (m_dispatchQueue.size() > 0)) { 
      reservation_station_t rs;
      const proc_inst_t& p_inst = m_dispatchQueue.front();

      rs.op_code = p_inst.op_code;
      rs.dest_reg = p_inst.dest_reg;
      for (int32_t i = 0; i < 2; ++i) {
        // check if the source register files are ready
        if ((p_inst.src_reg[i] < 0) || (m_regFile[p_inst.src_reg[i]].first)) {
          rs.src_reg_ready[i] = true;
        }
        else {
          // store the tag of the file, if it is not ready
          rs.src_reg_tag[i] = m_regFile[p_inst.src_reg[i]].second;
          rs.src_reg_ready[i] = false;
        }
      }
      if (p_inst.dest_reg >= 0) {
        // mark register for destination register as not ready
        m_regFile[p_inst.dest_reg] = std::make_pair(false, p_inst.tag);
      }
      rs.dest_reg_tag = p_inst.tag;
      rs.status = DISPATCHED;
      rs.clock_stamp = p_stats->cycle_count;

      // insert the instruction in scheduling queue
      m_schedulingQueue.insert(std::make_pair(rs.dest_reg_tag, rs));
      // update the instruction cycle log for this instruction's schedule cycle
      m_instructionCycleLog[p_inst.tag][2] = (p_stats->cycle_count + 1);
#if DEBUG_LOG
      std::cerr << p_stats->cycle_count << "\tDISPATCHED\t" << (p_inst.tag + 1) << std::endl;
#endif
      // remove the scheduled instruction from dispatch queue
      m_dispatchQueue.pop();

      --m_reservedSlots;
    }
  }
}

/**
 * @brief Function which schedules instructions.
 *
 * @param p_stats     Pointer to the statistics structure. 
 * @param firstHalf   Variable for indicating if it is the first half of the cycle.
 */
void
TomasuloSimulator::schedule(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  for (std::map<uint32_t, reservation_station_t>::iterator qe = m_schedulingQueue.begin(); qe != m_schedulingQueue.end(); ++qe) {
    reservation_station_t& rs = qe->second;
    // dispatched instructions may still be in the queue
    // so can be the instructions which were added in this cycle
    // need to ignore both of them
    if ((rs.status != DISPATCHED) || (rs.clock_stamp == p_stats->cycle_count)) {
      continue;
    }
    if (firstHalf) {
      // if both the source registers are ready
      if (rs.src_reg_ready[0] && rs.src_reg_ready[1]) {
        // use functional unit 1 for instructions of type -1, as per instructions
        int32_t op_code = (rs.op_code == -1) ? 1: rs.op_code;
        // find a free slot in the functional unit
        std::vector<int32_t>::iterator fu = std::find(m_scoreboard[op_code].begin(), m_scoreboard[op_code].end(), -1);
        if (fu != m_scoreboard[op_code].end()) {
          // schedule the instruction if a free functional unit is found
          *fu = qe->first;
          rs.status = SCHEDULED;
          rs.clock_stamp = p_stats->cycle_count;
          // update the instruction cycle log
          m_instructionCycleLog[rs.dest_reg_tag][3] = (p_stats->cycle_count + 1);
#if DEBUG_LOG
          std::cerr << p_stats->cycle_count << "\tSCHEDULED\t" << (rs.dest_reg_tag + 1) << std::endl;
#endif
          m_firedInstruction += 1;
        }
      }
    }
    else {
      // update source registers via broadcasted results on cdb
      for (std::vector<result_bus_t>::const_iterator cdb = m_resultBuses.begin(); cdb != m_resultBuses.end(); ++cdb) {
        if (cdb->busy) {
          for (int32_t i = 0; i < 2; ++i) {
            if (cdb->tag == rs.src_reg_tag[i]) {
              rs.src_reg_ready[i] = true;
            }
          }
        }
      }
    }
  }
}

/**
 * @brief Function which executes instructions.
 *
 * @param p_stats     Pointer to the statistics structure. 
 * @param firstHalf   Variable for indicating if it is the first half of the cycle.
 */
void
TomasuloSimulator::execute(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  if (firstHalf) {
    std::map<uint32_t, std::pair<uint32_t, reservation_station_t*> > executedInstructions;
    for (uint32_t op_code = 0; op_code < NUM_FU_TYPES; ++op_code) {
      for (std::vector<int32_t>::iterator i = m_scoreboard[op_code].begin(); i != m_scoreboard[op_code].end(); ++i) {
        if (*i >= 0) {
          uint32_t tag = static_cast<uint32_t>(*i);
          reservation_station_t* rs = &m_schedulingQueue[tag];
          if (rs->status == SCHEDULED) {
            // mark scheduled instructions as executed and push them to executed instructions
            rs->status = EXECUTED;
            rs->clock_stamp = p_stats->cycle_count;
#if DEBUG_LOG
            std::cerr << p_stats->cycle_count << "\tEXECUTED\t" << (tag + 1) << std::endl;
#endif
            executedInstructions.insert(std::make_pair(tag, std::make_pair(op_code, rs)));
          }
        }
      }
    }
    // push all the instructions executed in this cycle to waiting instructions' queue
    for (std::map<uint32_t, std::pair<uint32_t, reservation_station_t*> >::const_iterator ex = executedInstructions.begin(); ex != executedInstructions.end(); ++ex) {
      m_waitingInstructions.push_back(ex->second);
    }

    // based on the availability of result buses, broadcast execution results and mark the instruction as complete
    std::vector<std::pair<uint32_t, reservation_station_t*> >::iterator w = m_waitingInstructions.begin();
    for (std::vector<result_bus_t>::iterator cdb = m_resultBuses.begin(); (cdb != m_resultBuses.end()) && (w != m_waitingInstructions.end()); ++cdb) {
      uint32_t op_code = w->first;
      reservation_station_t* r = w->second;

      cdb->busy = false;
      if ((w->second)->dest_reg >= 0) {
        cdb->busy = true;
        cdb->tag = r->dest_reg_tag;
        cdb->reg = r->dest_reg;
        // update the register file
        if (cdb->tag == m_regFile[cdb->reg].second) {
          m_regFile[cdb->reg].first = true;
        }
      }

      // free the functional unit
      std::vector<int32_t>::iterator fu = std::find(m_scoreboard[op_code].begin(), m_scoreboard[op_code].end(), static_cast<int32_t>(r->dest_reg_tag));
      *fu = -1;

      m_schedulingQueue[r->dest_reg_tag].status = COMPLETED;
      m_schedulingQueue[r->dest_reg_tag].clock_stamp = p_stats->cycle_count;

      w = m_waitingInstructions.erase(w);
    }
  }
}

/**
 * @brief Function which retires instructions.
 *
 * @param p_stats     Pointer to the statistics structure. 
 * @param firstHalf   Variable for indicating if it is the first half of the cycle.
 */
void
TomasuloSimulator::stateUpdate(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  if (!firstHalf) {
    std::map<uint32_t, reservation_station_t>::iterator qe = m_schedulingQueue.begin();
    while (qe != m_schedulingQueue.end()) {
      if (qe->second.clock_stamp < p_stats->cycle_count && qe->second.status == COMPLETED) {
        // update instruction cycle log
        m_instructionCycleLog[qe->first][4] = p_stats->cycle_count;
#if DEBUG_LOG
        std::cerr << p_stats->cycle_count << "\tSTATE UPDATE\t" << (qe->first + 1) << std::endl;
#endif
        // delete the instruction from scheduling queue
        m_schedulingQueue.erase(qe++);
        ++m_retiredInstruction;
      }
      else {
        ++qe;
      }
    }
  }
}

/**
 * @brief Function which simulates the processor.
 *
 * @param p_stats   Pointer to the statistics structure. 
 */
void
TomasuloSimulator::simulateProcessor(
  proc_stats_t* const p_stats
)
{
#if DEBUG_LOG
  std::cerr << "CYCLE\tOPERATION\tINSTRUCTION" << std::endl;
#endif

  while (!done()) {

    ++(p_stats->cycle_count);

    bool firstHalf = false;

    // Loop over all the stages twice for simulating
    // each half cycle behavior.
    do {

      firstHalf = !firstHalf;

      stateUpdate(p_stats, firstHalf);

      execute(p_stats, firstHalf);

      schedule(p_stats, firstHalf);

      dispatch(p_stats, firstHalf);

      fetch(p_stats, firstHalf);

    } while (firstHalf);

  }
}

/**
 * @brief   Function which prints cycle by cycle output in the standard format.
 */
void
TomasuloSimulator::printInstructionCycles(
) const
{
  std::cout << "INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE" << std::endl;
  uint64_t instCount = 1;
  for (std::vector<std::array<unsigned long, NUM_STAGES> >::const_iterator inst = m_instructionCycleLog.begin(); inst != m_instructionCycleLog.end(); ++inst, ++instCount) {
    const std::array<unsigned long, NUM_STAGES>& instCycle = *inst;
    std::cout << instCount << '\t' << instCycle[0] << '\t' << instCycle[1] << '\t' << instCycle[2] << '\t' << instCycle[3] << '\t' << instCycle[4] << std::endl;
  }
  std::cout << std::endl;
}
