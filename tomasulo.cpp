#include "tomasulo.hpp"

#include <algorithm>
#include <iostream>

TomasuloSimulator::TomasuloSimulator(
) : m_dispatchQueue(),
  m_resultBuses(0),
  m_schedulingQueueCapacity(0),
  m_fetchRate(0),
  m_dispatchQueueSize(0),
  m_firedInstruction(0),
  m_retiredInstruction(0),
  m_doneFetching(true)
{
}

TomasuloSimulator::TomasuloSimulator(
  const uint64_t r,
  const uint64_t k0,
  const uint64_t k1,
  const uint64_t k2,
  const uint64_t f
) : m_dispatchQueue(),
  m_resultBuses(r),
  m_schedulingQueueCapacity(2 * (k0 + k1 + k2)),
  m_fetchRate(f),
  m_dispatchQueueSize(0),
  m_firedInstruction(0),
  m_doneFetching(false)
{
  for (uint64_t i = 0; i < k0; ++i) {
    m_scoreboard[0].push_back(-1);
  }
  for (uint64_t i = 0; i < k1; ++i) {
    m_scoreboard[1].push_back(-1);
  }
  for (uint64_t i = 0; i < k2; ++i) {
    m_scoreboard[2].push_back(-1);
  }

  for (uint64_t i = 0; i < NUM_REGISTERS; ++i) {
    m_regFile[i].first = true;
  }
}

TomasuloSimulator::TomasuloSimulator(
  const TomasuloSimulator& ts
)
{
  m_dispatchQueue = ts.m_dispatchQueue;
  m_resultBuses = ts.m_resultBuses;
  m_schedulingQueueCapacity = ts.m_schedulingQueueCapacity,
  m_fetchRate = ts.m_fetchRate;
  m_dispatchQueueSize = ts.m_dispatchQueueSize;
  m_firedInstruction = ts.m_firedInstruction;
  m_retiredInstruction = ts.m_retiredInstruction;
  m_doneFetching = ts.m_doneFetching;

  for (uint64_t i = 0; i < NUM_FU_TYPES; ++i) {
    m_scoreboard[i] = ts.m_scoreboard[i];
  }
  for (uint64_t i = 0; i < NUM_REGISTERS; ++i) {
    m_regFile[i] = ts.m_regFile[i];
  }
}

void
TomasuloSimulator::fetch(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  if (!firstHalf) {
    static uint32_t counter = 0;
    for (uint64_t f = 0; f < m_fetchRate; ++f) {
      proc_inst_t p_inst;
      if (read_instruction(&p_inst)) {
        p_inst.tag = counter++;
        m_dispatchQueue.push(p_inst);
        std::cerr << p_stats->cycle_count << " FETCHED " << (p_inst.tag + 1) << std::endl;
      }
      else {
        m_doneFetching = true;
        break;
      }
    }

    m_dispatchQueueSize += m_dispatchQueue.size(); 
    if (m_dispatchQueue.size() > p_stats->max_disp_size) {
      p_stats->max_disp_size = static_cast<unsigned long>(m_dispatchQueue.size());
    }
  }
}

void
TomasuloSimulator::dispatch(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  static uint64_t reserved_slots = 0;
  if (firstHalf) {
    reserved_slots = std::min(m_schedulingQueueCapacity - m_schedulingQueue.size(), m_dispatchQueue.size());
  }
  else {
    while ((reserved_slots > 0) && (m_dispatchQueue.size() > 0)) { 
      reservation_station_t rs;
      const proc_inst_t& p_inst = m_dispatchQueue.front();

      rs.op_code = p_inst.op_code;
      rs.dest_reg = p_inst.dest_reg;
      for (int32_t i = 0; i < 2; ++i) {
        if ((p_inst.src_reg[i] < 0) || (m_regFile[p_inst.src_reg[i]].first)) {
          rs.src_reg_ready[i] = true;
        }
        else {
          rs.src_reg_tag[i] = m_regFile[p_inst.src_reg[i]].second;
          rs.src_reg_ready[i] = false;
        }
      }
      if (p_inst.dest_reg >= 0) {
        m_regFile[p_inst.dest_reg] = std::make_pair(false, p_inst.tag);
      }
      rs.dest_reg_tag = p_inst.tag;
      rs.status = DISPATCHED;
      rs.clock_stamp = p_stats->cycle_count;

      m_schedulingQueue.insert(std::make_pair(rs.dest_reg_tag, rs));
      std::cerr << p_stats->cycle_count << " DISPATCHED " << (p_inst.tag + 1) << std::endl;
      m_dispatchQueue.pop();

      --reserved_slots;
    }
  }
}

void
TomasuloSimulator::schedule(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  for (std::map<uint32_t, reservation_station_t>::iterator qe = m_schedulingQueue.begin(); qe != m_schedulingQueue.end(); ++qe) {
    reservation_station_t& rs = qe->second;
    if ((rs.status != DISPATCHED) || (rs.clock_stamp == p_stats->cycle_count)) {
      continue;
    }
    if (firstHalf) {
      if (rs.src_reg_ready[0] && rs.src_reg_ready[1]) {
        int32_t op_code = (rs.op_code == -1) ? 1: rs.op_code;
        std::vector<int32_t>::iterator fu = std::find(m_scoreboard[op_code].begin(), m_scoreboard[op_code].end(), -1);
        if (fu != m_scoreboard[op_code].end()) {
          *fu = qe->first;
          rs.status = SCHEDULED;
          rs.clock_stamp = p_stats->cycle_count;
          std::cerr << p_stats->cycle_count << " SCHEDULED " << (rs.dest_reg_tag + 1) << std::endl;
          m_firedInstruction += 1;
        }
      }
    }
    else {
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

void
TomasuloSimulator::execute(
  proc_stats_t* const p_stats,
  const bool firstHalf
)
{
  if (firstHalf) {
    static std::vector<std::pair<uint32_t, reservation_station_t*> > waiting_instructions;

    std::map<uint32_t, std::pair<uint32_t, reservation_station_t*> > executed_instructions;
    for (uint32_t op_code = 0; op_code < NUM_FU_TYPES; ++op_code) {
      for (std::vector<int32_t>::iterator i = m_scoreboard[op_code].begin(); i != m_scoreboard[op_code].end(); ++i) {
        if (*i >= 0) {
          uint32_t tag = static_cast<uint32_t>(*i);
          reservation_station_t* rs = &m_schedulingQueue[tag];
          if (rs->status == SCHEDULED) {
            rs->status = EXECUTED;
            rs->clock_stamp = p_stats->cycle_count;
            std::cerr << p_stats->cycle_count << " EXECUTED " << (tag + 1) << std::endl;
            executed_instructions.insert(std::make_pair(tag, std::make_pair(op_code, rs)));
          }
        }
      }
    }
    for (std::map<uint32_t, std::pair<uint32_t, reservation_station_t*> >::const_iterator ex = executed_instructions.begin(); ex != executed_instructions.end(); ++ex) {
      waiting_instructions.push_back(ex->second);
    }

    std::vector<std::pair<uint32_t, reservation_station_t*> >::iterator w = waiting_instructions.begin();
    for (std::vector<result_bus_t>::iterator cdb = m_resultBuses.begin(); (cdb != m_resultBuses.end()) && (w != waiting_instructions.end()); ++cdb) {
      uint32_t op_code = w->first;
      reservation_station_t* r = w->second;

      cdb->busy = false;
      if ((w->second)->dest_reg >= 0) {
        cdb->busy = true;
        cdb->tag = r->dest_reg_tag;
        cdb->reg = r->dest_reg;
        if (cdb->tag == m_regFile[cdb->reg].second) {
          m_regFile[cdb->reg].first = true;
        }
      }

      std::vector<int32_t>::iterator fu = std::find(m_scoreboard[op_code].begin(), m_scoreboard[op_code].end(), static_cast<int32_t>(r->dest_reg_tag));
      *fu = -1;

      m_schedulingQueue[r->dest_reg_tag].status = COMPLETED;
      m_schedulingQueue[r->dest_reg_tag].clock_stamp = p_stats->cycle_count;

      w = waiting_instructions.erase(w);
    }
  }
}

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
        std::cerr << p_stats->cycle_count << " STATE UPDATE " << (qe->first + 1) << std::endl;
        m_schedulingQueue.erase(qe++);
        ++m_retiredInstruction;
      }
      else {
        ++qe;
      }
    }
  }
}

void
TomasuloSimulator::simulateProcessor(
  proc_stats_t* const p_stats
)
{
  std::cerr << "CYCLE OPERATION INSTRUCTION" << std::endl;

  while (!done()) {

    ++(p_stats->cycle_count);

    bool firstHalf = false;

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
