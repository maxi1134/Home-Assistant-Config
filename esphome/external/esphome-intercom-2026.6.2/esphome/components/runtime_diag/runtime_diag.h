#pragma once

#include "esphome/core/component.h"

#include <atomic>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace runtime_diag {

class RuntimeDiag : public Component {
 public:
  void set_dump_interval(uint32_t interval_ms) { this->dump_interval_ms_ = interval_ms; }
  void set_dump_window(uint32_t window_ms) { this->dump_window_ms_ = window_ms; }
  void set_loop_watch(bool enabled) { this->loop_watch_enabled_ = enabled; }
  void set_loop_watch_interval(uint32_t interval_ms) { this->loop_watch_interval_ms_ = interval_ms; }
  void set_loop_stall_threshold(uint32_t threshold_ms) { this->loop_stall_threshold_ms_ = threshold_ms; }
  void set_dump_backtraces(bool enabled) { this->dump_backtraces_enabled_ = enabled; }
  void set_dump_network(bool enabled) { this->dump_network_enabled_ = enabled; }
  void set_periodic_dump(bool enabled) { this->periodic_dump_enabled_ = enabled; }
  void set_task_log_limit(uint32_t limit) { this->task_log_limit_ = limit; }
  void set_loop_watch_stack_size(uint32_t stack_bytes) { this->loop_watch_stack_bytes_ = stack_bytes; }
  void set_loop_watch_priority(uint32_t priority) { this->loop_watch_priority_ = priority; }

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void mark_api_connected(const char *reason = "api_connected");
  void mark_api_disconnected(const char *reason = "api_disconnected");
  void dump_now(const char *reason = "manual");
  void print_twdt_isr() const;

 protected:
  void dump_(const char *reason);
  void dump_network_();
  void dump_heap_();
  void dump_tasks_();
  void dump_task_backtraces_();
  void start_loop_watch_();
  static void loop_watch_task_(void *arg);
  void loop_watch_();

  struct TaskRuntimeSample {
    uint32_t task_number{0};
    uint32_t runtime{0};
    bool used{false};
  };

  static constexpr size_t MAX_TASK_SAMPLES = 64;
  TaskRuntimeSample previous_tasks_[MAX_TASK_SAMPLES]{};
  uint32_t previous_total_runtime_{0};
  int64_t previous_task_sample_us_{0};
  bool have_previous_tasks_{false};

  uint32_t dump_interval_ms_{5000};
  uint32_t dump_window_ms_{180000};
  uint32_t dump_until_ms_{0};
  uint32_t last_dump_ms_{0};
  bool api_disconnected_{false};
  bool dump_network_enabled_{true};
  bool periodic_dump_enabled_{false};
  uint32_t task_log_limit_{64};

  std::atomic<uint32_t> loop_heartbeat_ms_{0};
  std::atomic<uint32_t> loop_heartbeat_tick_{0};
  std::atomic<uint32_t> loop_watch_tick_{0};
  std::atomic<uint32_t> loop_watch_iterations_{0};
  std::atomic<uint32_t> last_loop_stall_dump_ms_{0};
  std::atomic_flag dumping_ = ATOMIC_FLAG_INIT;
  TaskHandle_t loop_watch_task_handle_{nullptr};
  uint32_t loop_watch_interval_ms_{500};
  uint32_t loop_stall_threshold_ms_{3000};
  uint32_t loop_watch_stack_bytes_{4096};
  uint32_t loop_watch_priority_{10};
  bool loop_watch_enabled_{false};
  bool dump_backtraces_enabled_{false};
};

}  // namespace runtime_diag
}  // namespace esphome
