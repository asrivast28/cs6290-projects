#include "procsim.hpp"

#include <algorithm>
#include <cinttypes>
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

static uint64_t fetch_rate = 0;
static uint64_t scheduling_queue_capacity = 0;

static std::vector<result_bus_t> result_buses;
static std::queue<proc_inst_t> dispatch_queue;
static std::vector<reservation_station_t> scheduling_queue;
static std::set<reservation_station_t*> scoreboard[FUNCTIONAL_UNIT_RANGE];

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
    scoreboard[0].insert(0);
  }
  for (uint64_t i = 0; i < k1; ++i) {
    scoreboard[1].insert(0);
  }
  for (uint64_t i = 0; i < k2; ++i) {
    scoreboard[2].insert(0);
  }

  scheduling_queue_capacity = 2 * (k0 + k1 + k2);
  for (int32_t i = 0; i < NUM_REGISTERS; ++i) {
    reg_file[i].first = true;
  }

  fetch_rate = f;
}

static void fetch()
{
  static uint32_t counter = 0;
  for (uint64_t f = 0; f < fetch_rate; ++f) {
    proc_inst_t p_inst;
    if (read_instruction(&p_inst)) {
      p_inst.tag = counter++;
      dispatch_queue.push(p_inst);
    }
    else {
      break;
    }
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
      if (reg_file[p_inst.src_reg[i]].first) {
        rs.src_reg_ready[i] = true;
      }
      else if (p_inst.src_reg[i] >= 0) {
        rs.src_reg_tag[i] = reg_file[p_inst.src_reg[i]].second;
        rs.src_reg_ready[i] = false;
      }
    }
    reg_file[p_inst.dest_reg] = std::make_pair(false, p_inst.tag);
    rs.dest_reg_tag = p_inst.tag;

    scheduling_queue.push_back(rs);
    dispatch_queue.pop();
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
  std::sort(scheduling_queue.begin(), scheduling_queue.end(), compare_tags);

  for (std::vector<reservation_station_t>::iterator rs = scheduling_queue.begin(); rs != scheduling_queue.end(); ++rs) {
    for (std::vector<result_bus_t>::const_iterator cdb = result_buses.begin(); cdb != result_buses.end(); ++cdb) {
      for (int32_t i = 0; i < 2; ++i) {
        if (cdb->tag == rs->src_reg_tag[i]) {
          rs->src_reg_ready[i] = true;
        }
      }
    }
    if (rs->src_reg_ready[0] && rs->src_reg_ready[1]) {
      std::set<reservation_station_t*>::iterator fu = scoreboard[rs->op_code].find(0);
      if (fu != scoreboard[rs->op_code].end()) {
        scoreboard[rs->op_code].erase(fu);
        scoreboard[rs->op_code].insert(&(*rs));
      }
    }
  }
}

static void execute()
{
  std::map<uint32_t, std::pair<uint32_t, reservation_station_t*> > waiting_instructions;
  for (uint32_t fu = 0; fu < FUNCTIONAL_UNIT_RANGE; ++fu) {
    for (std::set<reservation_station_t*>::iterator i = scoreboard[fu].begin(); i != scoreboard[fu].end(); ++i) {
      if (*i != 0) {
        waiting_instructions.insert(std::make_pair((*i)->dest_reg_tag, std::make_pair(fu, *i)));
      }
    }
  }

  std::map<uint32_t, std::pair<uint32_t, reservation_station_t*> >::iterator i = waiting_instructions.begin();
  for (std::vector<result_bus_t>::iterator cdb = result_buses.begin(); cdb != result_buses.end(); ++cdb) {
    if (!cdb->busy) {
      uint32_t fu = (i->second).first;
      reservation_station_t* r = (i->second).second;

      cdb->busy = true;
      cdb->tag = i->first;
      cdb->reg = r->dest_reg;
      std::vector<reservation_station_t>::iterator rs = std::find(scheduling_queue.begin(), scheduling_queue.end(), *r);
      scheduling_queue.erase(rs);

      std::set<reservation_station_t*>::iterator it = scoreboard[fu].find(r);
      scoreboard[fu].erase(it);
      scoreboard[fu].insert(0);

      ++i;
    }
  }
}

static void state_update()
{
  for (std::vector<result_bus_t>::iterator cdb = result_buses.begin(); cdb != result_buses.end(); ++cdb) {
    if (cdb->busy && (cdb->tag = reg_file[cdb->reg].second)) {
      reg_file[cdb->reg].first = true;
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
  uint64_t clock = 0;

  do {

    state_update();

    execute();

    schedule();

    dispatch();

    fetch();

    ++clock;

  } while (scheduling_queue.size() > 0);

}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) 
{

}
