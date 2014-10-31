#include "procsim.hpp"

#include <algorithm>
#include <cinttypes>
#include <iostream>
#include <queue>
#include <map>
#include <set>

#define NUM_REGISTERS 128
#define FUNCTIONAL_UNIT_RANGE 3

typedef struct _reservation_station_t {
  int32_t op_code;
  bool src_reg_ready[2];
  uint32_t src_reg_tag[2];
  int32_t dest_reg;
  uint32_t dest_reg_tag;

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

static std::vector<result_bus_t> result_buses;
static std::queue<proc_inst_t> dispatch_queue;
static std::vector<std::pair<reservation_station_t, bool> > scheduling_queue;
static std::vector<reservation_station_t*> scoreboard[FUNCTIONAL_UNIT_RANGE];

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
    cdb.busy = false;
    result_buses.push_back(cdb);
  }

  for (uint64_t i = 0; i < k0; ++i) {
    scoreboard[0].push_back(0);
  }
  for (uint64_t i = 0; i < k1; ++i) {
    scoreboard[1].push_back(0);
  }
  for (uint64_t i = 0; i < k2; ++i) {
    scoreboard[2].push_back(0);
  }

  scheduling_queue_capacity = 2 * (k0 + k1 + k2);
  for (int32_t i = 0; i < NUM_REGISTERS; ++i) {
    reg_file[i].first = true;
  }

  fetch_rate = f;
}

static void fetch(proc_stats_t* p_stats)
{
  static uint32_t counter = 0;
  for (uint64_t f = 0; f < fetch_rate; ++f) {
    proc_inst_t p_inst;
    if (read_instruction(&p_inst)) {
      p_inst.tag = counter++;
      dispatch_queue.push(p_inst);
      std::cerr << "FETCHED " << p_inst.tag << std::endl;
    }
    else {
      done_fetching = true;
      break;
    }
  }

  p_stats->avg_disp_size += dispatch_queue.size(); 
  if (dispatch_queue.size() > p_stats->max_disp_size) {
    p_stats->max_disp_size = static_cast<unsigned long>(dispatch_queue.size());
  }
}

static void dispatch()
{
  while ((scheduling_queue.size() < scheduling_queue_capacity) && (dispatch_queue.size() > 0)) {
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

    scheduling_queue.push_back(std::make_pair(rs, false));
    dispatch_queue.pop();
    std::cerr << "DISPATCHED " << p_inst.tag << std::endl;
  }
}

static bool compare_tags(const reservation_station_t& rs1, const reservation_station_t& rs2)
{
  return (rs1.dest_reg_tag < rs2.dest_reg_tag);
}

static void schedule()
{
  // Sort the scheduling queue so that instructions with lower tag values are fired first.
  // XXX: This will probably happen automatically.
  // std::sort(scheduling_queue.begin(), scheduling_queue.end(), compare_tags);

  for (std::vector<std::pair<reservation_station_t, bool> >::iterator rs = scheduling_queue.begin(); rs != scheduling_queue.end(); ++rs) {
    for (std::vector<result_bus_t>::const_iterator cdb = result_buses.begin(); cdb != result_buses.end(); ++cdb) {
      for (int32_t i = 0; i < 2; ++i) {
        if (cdb->tag == rs->first.src_reg_tag[i]) {
          rs->first.src_reg_ready[i] = true;
        }
      }
    }
    if (!rs->second && rs->first.src_reg_ready[0] && rs->first.src_reg_ready[1]) {
      int32_t op_code = (rs->first.op_code == -1) ? 1: rs->first.op_code;
      std::vector<reservation_station_t*>::iterator fu = std::find(scoreboard[op_code].begin(), scoreboard[op_code].end(), static_cast<reservation_station_t*>(0));
      if (fu != scoreboard[op_code].end()) {
        *fu = &(rs->first);
        rs->second = true;
        std::cerr << "SCHEDULED " << rs->first.dest_reg_tag << std::endl;
      }
    }
  }
}

static void execute()
{
  std::vector<std::pair<uint32_t, reservation_station_t*> > waiting_instructions;
  for (uint32_t fu = 0; fu < FUNCTIONAL_UNIT_RANGE; ++fu) {
    for (std::vector<reservation_station_t*>::iterator i = scoreboard[fu].begin(); i != scoreboard[fu].end(); ++i) {
      if (*i != 0) {
        waiting_instructions.push_back(std::make_pair(fu, *i));
      }
    }
  }

  if (waiting_instructions.size() > 0) {
    std::vector<std::pair<uint32_t, reservation_station_t*> >::iterator w = waiting_instructions.begin();
    for (std::vector<result_bus_t>::iterator cdb = result_buses.begin(); cdb != result_buses.end(); ++cdb) {
      if (!cdb->busy) {
        uint32_t fu = w->first;
        reservation_station_t* r = w->second;

        if (r->dest_reg >= 0) {
          cdb->busy = true;
          cdb->tag = r->dest_reg_tag;
          cdb->reg = r->dest_reg;
          std::cout << r->dest_reg_tag << " " << r->dest_reg << std::endl;
        }

        for (std::vector<std::pair<reservation_station_t, bool> >::iterator rs = scheduling_queue.begin(); rs != scheduling_queue.end(); ++rs) {
          if (rs->first == *r) {
            scheduling_queue.erase(rs);
            break;
          }
        }

        std::vector<reservation_station_t*>::iterator it = std::find(scoreboard[fu].begin(), scoreboard[fu].end(), r);
        if (it != scoreboard[fu].end()) {
          *it = 0;
        }

        ++w;
      }
    }
  }
}

static void state_update()
{
  for (std::vector<result_bus_t>::iterator cdb = result_buses.begin(); cdb != result_buses.end(); ++cdb) {
    if (cdb->busy && (cdb->tag = reg_file[cdb->reg].second)) {
      reg_file[cdb->reg].first = true;
      cdb->busy = false;
      std::cout << "STATE UPDATE " << cdb->tag << std::endl;
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
  while (!done_fetching || (scheduling_queue.size() > 0)) {

    state_update();

    execute();

    schedule();

    dispatch();

    fetch(p_stats);

    ++(p_stats->cycle_count);

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
  p_stats->avg_inst_retired = p_stats->retired_instruction / p_stats->cycle_count;
  p_stats->avg_inst_fired = p_stats->retired_instruction / p_stats->cycle_count;
  p_stats->avg_disp_size = p_stats->avg_disp_size / static_cast<float>(p_stats->cycle_count);
}
