#include "procsim.hpp"

#include <algorithm>
#include <cinttypes>
#include <iostream>
#include <queue>
#include <map>
#include <set>

#define NUM_REGISTERS 128
#define FUNCTIONAL_UNIT_RANGE 3

enum schedule_status_t {
  DISPATCHED,
  SCHEDULED,
  EXECUTED,
  COMPLETED
};

typedef struct _reservation_station_t {
  int32_t op_code;
  bool src_reg_ready[2];
  uint32_t src_reg_tag[2];
  int32_t dest_reg;
  uint32_t dest_reg_tag;

  unsigned long clock_stamp;
  schedule_status_t status;

  bool operator==(const _reservation_station_t& rs) const
  {
    return (dest_reg_tag == rs.dest_reg_tag);
  }

} reservation_station_t;

typedef struct _result_bus_t {
  bool busy;
  uint32_t tag;
  int32_t reg;
} result_bus_t;

static bool done_fetching = false;
static uint64_t fetch_rate = 0;
static uint64_t scheduling_queue_capacity = 0;
static unsigned long fired_instruction = 0;
static unsigned long dispatch_queue_size = 0;

static std::vector<result_bus_t> result_buses;
static std::queue<proc_inst_t> dispatch_queue;
static std::map<uint32_t, reservation_station_t> scheduling_queue;
static std::vector<int32_t> scoreboard[FUNCTIONAL_UNIT_RANGE];

static std::pair<bool, uint32_t> reg_file[NUM_REGISTERS];

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
  for (uint64_t i = 0; i < r; ++i) {
    result_bus_t cdb;
    result_buses.push_back(cdb);
  }

  for (uint64_t i = 0; i < k0; ++i) {
    scoreboard[0].push_back(-1);
  }
  for (uint64_t i = 0; i < k1; ++i) {
    scoreboard[1].push_back(-1);
  }
  for (uint64_t i = 0; i < k2; ++i) {
    scoreboard[2].push_back(-1);
  }

  scheduling_queue_capacity = 2 * (k0 + k1 + k2);
  for (int32_t i = 0; i < NUM_REGISTERS; ++i) {
    reg_file[i].first = true;
  }

  fetch_rate = f;
}

static void fetch(proc_stats_t* p_stats, const bool first_half)
{
  if (!first_half) {
    static uint32_t counter = 0;
    for (uint64_t f = 0; f < fetch_rate; ++f) {
      proc_inst_t p_inst;
      if (read_instruction(&p_inst)) {
        p_inst.tag = counter++;
        dispatch_queue.push(p_inst);
        std::cerr << p_stats->cycle_count << " FETCHED " << (p_inst.tag + 1) << std::endl;
      }
      else {
        done_fetching = true;
        break;
      }
    }

    dispatch_queue_size += dispatch_queue.size(); 
    if (dispatch_queue.size() > p_stats->max_disp_size) {
      p_stats->max_disp_size = static_cast<unsigned long>(dispatch_queue.size());
    }
  }
}

static void dispatch(proc_stats_t* p_stats, const bool first_half)
{
  static uint64_t reserved_slots = 0;
  if (first_half) {
    reserved_slots = std::min(scheduling_queue_capacity - scheduling_queue.size(), dispatch_queue.size());
  }
  else {
    while ((reserved_slots > 0) && (dispatch_queue.size() > 0)) { 
      reservation_station_t rs;
      const proc_inst_t& p_inst = dispatch_queue.front();

      rs.op_code = p_inst.op_code;
      rs.dest_reg = p_inst.dest_reg;
      for (int32_t i = 0; i < 2; ++i) {
        if ((p_inst.src_reg[i] < 0) || (reg_file[p_inst.src_reg[i]].first)) {
          rs.src_reg_ready[i] = true;
        }
        else {
          rs.src_reg_tag[i] = reg_file[p_inst.src_reg[i]].second;
          rs.src_reg_ready[i] = false;
        }
      }
      if (p_inst.dest_reg >= 0) {
        reg_file[p_inst.dest_reg] = std::make_pair(false, p_inst.tag);
      }
      rs.dest_reg_tag = p_inst.tag;
      rs.status = DISPATCHED;
      rs.clock_stamp = p_stats->cycle_count;

      scheduling_queue.insert(std::make_pair(rs.dest_reg_tag, rs));
      std::cerr << p_stats->cycle_count << " DISPATCHED " << (p_inst.tag + 1) << std::endl;
      dispatch_queue.pop();

      --reserved_slots;
    }
  }
}

static void schedule(proc_stats_t* p_stats, const bool first_half)
{
  for (std::map<uint32_t, reservation_station_t>::iterator qe = scheduling_queue.begin(); qe != scheduling_queue.end(); ++qe) {
    reservation_station_t& rs = qe->second;
    if ((rs.status != DISPATCHED) || (rs.clock_stamp == p_stats->cycle_count)) {
      continue;
    }
    if (first_half) {
      if (rs.src_reg_ready[0] && rs.src_reg_ready[1]) {
        int32_t op_code = (rs.op_code == -1) ? 1: rs.op_code;
        std::vector<int32_t>::iterator fu = std::find(scoreboard[op_code].begin(), scoreboard[op_code].end(), -1);
        if (fu != scoreboard[op_code].end()) {
          *fu = qe->first;
          rs.status = SCHEDULED;
          rs.clock_stamp = p_stats->cycle_count;
          std::cerr << p_stats->cycle_count << " SCHEDULED " << (rs.dest_reg_tag + 1) << std::endl;
          fired_instruction += 1;
        }
      }
    }
    else {
      for (std::vector<result_bus_t>::const_iterator cdb = result_buses.begin(); cdb != result_buses.end(); ++cdb) {
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

static void execute(proc_stats_t* p_stats, const bool first_half)
{
  if (first_half) {
    static std::vector<std::pair<uint32_t, reservation_station_t*> > waiting_instructions;

    std::map<uint32_t, std::pair<uint32_t, reservation_station_t*> > executed_instructions;
    for (uint32_t op_code = 0; op_code < FUNCTIONAL_UNIT_RANGE; ++op_code) {
      for (std::vector<int32_t>::iterator i = scoreboard[op_code].begin(); i != scoreboard[op_code].end(); ++i) {
        if (*i >= 0) {
          uint32_t tag = static_cast<uint32_t>(*i);
          reservation_station_t* rs = &scheduling_queue[tag];
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
    for (std::vector<result_bus_t>::iterator cdb = result_buses.begin(); (cdb != result_buses.end()) && (w != waiting_instructions.end()); ++cdb) {
      uint32_t op_code = w->first;
      reservation_station_t* r = w->second;

      cdb->busy = false;
      if ((w->second)->dest_reg >= 0) {
        cdb->busy = true;
        cdb->tag = r->dest_reg_tag;
        cdb->reg = r->dest_reg;
        if (cdb->tag == reg_file[cdb->reg].second) {
          reg_file[cdb->reg].first = true;
        }
      }

      std::vector<int32_t>::iterator fu = std::find(scoreboard[op_code].begin(), scoreboard[op_code].end(), static_cast<int32_t>(r->dest_reg_tag));
      *fu = -1;

      scheduling_queue[r->dest_reg_tag].status = COMPLETED;
      scheduling_queue[r->dest_reg_tag].clock_stamp = p_stats->cycle_count;

      w = waiting_instructions.erase(w);
    }
  }
}

static void state_update(proc_stats_t* p_stats, const bool first_half)
{
  if (!first_half) {
    std::map<uint32_t, reservation_station_t>::iterator qe = scheduling_queue.begin();
    while (qe != scheduling_queue.end()) {
      if (qe->second.clock_stamp < p_stats->cycle_count && qe->second.status == COMPLETED) {
        std::cerr << p_stats->cycle_count << " STATE UPDATE " << (qe->first + 1) << std::endl;
        scheduling_queue.erase(qe++);
        ++(p_stats->retired_instruction);
      }
      else {
        ++qe;
      }
    }
  }
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats)
{
  std::cerr << "CYCLE OPERATION INSTRUCTION" << std::endl;

  while (!done_fetching || (scheduling_queue.size() > 0)) {

    ++(p_stats->cycle_count);

    bool first_half = false;

    do {

      first_half = !first_half;

      state_update(p_stats, first_half);

      execute(p_stats, first_half);

      schedule(p_stats, first_half);

      dispatch(p_stats, first_half);

      fetch(p_stats, first_half);

    } while (first_half);

  }

}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) 
{
  p_stats->avg_inst_retired = p_stats->retired_instruction / static_cast<float>(p_stats->cycle_count);
  p_stats->avg_inst_fired = fired_instruction / static_cast<float>(p_stats->cycle_count);
  p_stats->avg_disp_size = dispatch_queue_size / static_cast<float>(p_stats->cycle_count);

  done_fetching = false;
  fetch_rate = 0;
  scheduling_queue_capacity = 0;
  fired_instruction = 0;
  dispatch_queue_size = 0;

  result_buses.clear();
  scheduling_queue.clear();
}
