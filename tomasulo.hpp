#include "procsim.hpp"

#include <array>
#include <map>
#include <queue>
#include <vector>

#define NUM_REGISTERS 128
#define NUM_STAGES 5
#define NUM_FU_TYPES 3

enum schedule_status_t {
  DISPATCHED,
  SCHEDULED,
  EXECUTED,
  COMPLETED
};

typedef struct _result_bus_t {
  bool busy;
  uint32_t tag;
  int32_t reg;
} result_bus_t;

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

class TomasuloSimulator {
public:
  TomasuloSimulator();

  TomasuloSimulator(const uint64_t, const uint64_t[NUM_FU_TYPES], const uint64_t);

  TomasuloSimulator(const TomasuloSimulator& ts);

  void simulateProcessor(proc_stats_t* const);

  void printInstructionCycles() const;

  unsigned long dispatchQueueSize() const { return m_dispatchQueueSize; }

  unsigned long firedInstruction() const { return m_firedInstruction; }

  unsigned long retiredInstruction() const { return m_firedInstruction; }

private:
  void fetch(proc_stats_t* const, const bool);

  void dispatch(proc_stats_t* const, const bool);

  void schedule(proc_stats_t* const, const bool);

  void execute(proc_stats_t* const, const bool);

  void stateUpdate(proc_stats_t* const, const bool);
  
  bool done() const { return m_doneFetching && (m_schedulingQueue.size() == 0); }

private:
  std::map<uint32_t, reservation_station_t> m_schedulingQueue;

  std::vector<std::array<unsigned long, NUM_STAGES> > m_instructionCycleLog;

  std::vector<std::pair<uint32_t, reservation_station_t*> > m_waitingInstructions;

  std::vector<result_bus_t> m_resultBuses;

  std::array<std::vector<int32_t>, NUM_FU_TYPES> m_scoreboard;

  std::array<std::pair<bool, uint32_t>, NUM_REGISTERS> m_regFile;

  std::queue<proc_inst_t> m_dispatchQueue;

  uint64_t m_schedulingQueueCapacity;
  uint64_t m_fetchRate;
  uint64_t m_reservedSlots;

  unsigned long m_dispatchQueueSize;
  unsigned long m_firedInstruction;
  unsigned long m_retiredInstruction;

  uint32_t m_counter;

  bool m_doneFetching;
};
