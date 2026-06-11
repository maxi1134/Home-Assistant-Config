#include "runtime_diag.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#include <algorithm>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_debug_helpers.h"
#include "esp_timer.h"
#include "esp_memory_utils.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace runtime_diag {

static const char *const TAG = "runtime_diag";
RuntimeDiag *global_runtime_diag = nullptr;

namespace {

const char *task_state_to_str(eTaskState state) {
  switch (state) {
    case eRunning:
      return "run";
    case eReady:
      return "ready";
    case eBlocked:
      return "block";
    case eSuspended:
      return "susp";
    case eDeleted:
      return "del";
    case eInvalid:
    default:
      return "invalid";
  }
}

const char *task_core_to_str(BaseType_t core, char *buf, size_t len) {
  if (core == tskNO_AFFINITY) {
    snprintf(buf, len, "any");
  } else {
    snprintf(buf, len, "%ld", static_cast<long>(core));
  }
  return buf;
}

const char *stack_region_to_str(const void *ptr) {
  if (ptr == nullptr) return "null";
  if (esp_ptr_external_ram(ptr)) return "psram";
  if (esp_ptr_internal(ptr)) return "internal";
  return "other";
}

void ip4_to_str(const esp_ip4_addr_t &ip, char *buf, size_t len) {
  snprintf(buf, len, IPSTR, IP2STR(&ip));
}

}  // namespace

void RuntimeDiag::setup() {
  global_runtime_diag = this;
  ESP_LOGI(TAG, "runtime diagnostics ready (interval=%ums window=%ums periodic=%s network=%s task_limit=%u loop_watch=%s)",
           (unsigned) this->dump_interval_ms_, (unsigned) this->dump_window_ms_,
           this->periodic_dump_enabled_ ? "yes" : "no",
           this->dump_network_enabled_ ? "yes" : "no",
           (unsigned) this->task_log_limit_,
           this->loop_watch_enabled_ ? "yes" : "no");
  this->loop_heartbeat_ms_.store(millis(), std::memory_order_relaxed);
  this->loop_heartbeat_tick_.store(xTaskGetTickCount(), std::memory_order_relaxed);
  if (this->periodic_dump_enabled_) {
    this->dump_until_ms_ = millis() + this->dump_window_ms_;
  }
  if (this->loop_watch_enabled_) {
    this->start_loop_watch_();
  }
}

void RuntimeDiag::loop() {
  const uint32_t now = millis();
  this->loop_heartbeat_ms_.store(now, std::memory_order_relaxed);
  this->loop_heartbeat_tick_.store(xTaskGetTickCount(), std::memory_order_relaxed);

#ifdef USE_API
  const bool api_connected = api::global_api_server != nullptr && api::global_api_server->is_connected();
  if (api_connected && this->api_disconnected_) {
    this->api_disconnected_ = false;
    this->dump_now("api_recovered");
  }
#endif

  const bool periodic_window = this->periodic_dump_enabled_ &&
                               static_cast<int32_t>(now - this->dump_until_ms_) < 0;
  const bool in_window = (this->api_disconnected_ || periodic_window) &&
                         static_cast<int32_t>(now - this->dump_until_ms_) < 0;
  if (!in_window) return;
  if (now - this->last_dump_ms_ < this->dump_interval_ms_) return;
  this->dump_(this->api_disconnected_ ? "api_disconnected_periodic" : "periodic");
}

void RuntimeDiag::mark_api_connected(const char *reason) {
  this->api_disconnected_ = false;
  this->dump_now(reason);
}

void RuntimeDiag::mark_api_disconnected(const char *reason) {
  const uint32_t now = millis();
  this->api_disconnected_ = true;
  this->dump_until_ms_ = now + this->dump_window_ms_;
  this->dump_(reason);
}

void RuntimeDiag::dump_now(const char *reason) { this->dump_(reason); }

void RuntimeDiag::dump_(const char *reason) {
  if (this->dumping_.test_and_set(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "snapshot skipped while another dump is active (reason=%s)", reason);
    return;
  }
  this->last_dump_ms_ = millis();

#ifdef USE_API
  const bool api_connected = api::global_api_server != nullptr && api::global_api_server->is_connected();
#else
  const bool api_connected = false;
#endif

  ESP_LOGI(TAG,
           "=== snapshot reason=%s uptime=%ums api_connected=%s reset_reason=%d ===",
           reason, (unsigned) millis(), api_connected ? "yes" : "no",
           static_cast<int>(esp_reset_reason()));
  if (this->dump_network_enabled_) {
    this->dump_network_();
  }
  this->dump_heap_();
  this->dump_tasks_();
  if (this->dump_backtraces_enabled_) {
    this->dump_task_backtraces_();
  }
  ESP_LOGI(TAG, "=== snapshot end ===");
  this->dumping_.clear(std::memory_order_release);
}

void RuntimeDiag::dump_network_() {
#ifdef USE_WIFI
  const bool wifi_connected = wifi::global_wifi_component != nullptr &&
                              wifi::global_wifi_component->is_connected();
  const bool wifi_disabled = wifi::global_wifi_component != nullptr &&
                             wifi::global_wifi_component->is_disabled();
  const int8_t rssi = wifi::global_wifi_component != nullptr
                          ? wifi::global_wifi_component->wifi_rssi()
                          : -128;
  const int32_t channel = wifi::global_wifi_component != nullptr
                              ? wifi::global_wifi_component->get_wifi_channel()
                              : 0;
#else
  const bool wifi_connected = false;
  const bool wifi_disabled = false;
  const int8_t rssi = -128;
  const int32_t channel = 0;
#endif

  wifi_ap_record_t ap_info{};
  esp_err_t ap_err = esp_wifi_sta_get_ap_info(&ap_info);
  wifi_ps_type_t ps_type = WIFI_PS_NONE;
  esp_err_t ps_err = esp_wifi_get_ps(&ps_type);

  esp_netif_ip_info_t ip_info{};
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_err_t ip_err = sta_netif != nullptr ? esp_netif_get_ip_info(sta_netif, &ip_info)
                                          : ESP_ERR_NOT_FOUND;
  const bool netif_up = sta_netif != nullptr && esp_netif_is_netif_up(sta_netif);
  const int netif_index = sta_netif != nullptr ? esp_netif_get_netif_impl_index(sta_netif) : -1;
  char ip[16] = "0.0.0.0";
  char gw[16] = "0.0.0.0";
  char mask[16] = "0.0.0.0";
  if (ip_err == ESP_OK) {
    ip4_to_str(ip_info.ip, ip, sizeof(ip));
    ip4_to_str(ip_info.gw, gw, sizeof(gw));
    ip4_to_str(ip_info.netmask, mask, sizeof(mask));
  }

  ESP_LOGI(TAG,
           "net: wifi_connected=%s disabled=%s rssi=%d channel=%ld ps=%d ps_info=%s ap_info=%s netif_up=%s netif_index=%d ip_info=%s ip=%s gw=%s mask=%s",
           wifi_connected ? "yes" : "no", wifi_disabled ? "yes" : "no",
           static_cast<int>(rssi), static_cast<long>(channel),
           static_cast<int>(ps_type), esp_err_to_name(ps_err),
           esp_err_to_name(ap_err), netif_up ? "yes" : "no", netif_index,
           esp_err_to_name(ip_err), ip, gw, mask);
  if (ap_err == ESP_OK) {
    ESP_LOGI(TAG, "net: ssid='%s' bssid=%02x:%02x:%02x:%02x:%02x:%02x primary=%u rssi=%d",
             reinterpret_cast<const char *>(ap_info.ssid), ap_info.bssid[0],
             ap_info.bssid[1], ap_info.bssid[2], ap_info.bssid[3],
             ap_info.bssid[4], ap_info.bssid[5], ap_info.primary,
             static_cast<int>(ap_info.rssi));
  }
}

void RuntimeDiag::dump_heap_() {
  ESP_LOGI(TAG,
           "heap: internal free=%u min=%u largest=%u psram free=%u min=%u largest=%u",
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

void RuntimeDiag::dump_tasks_() {
#if configUSE_TRACE_FACILITY
  constexpr UBaseType_t MAX_TASKS = RuntimeDiag::MAX_TASK_SAMPLES;
  TaskStatus_t tasks[MAX_TASKS];
  uint32_t total_runtime = 0;
  const int64_t now_us = esp_timer_get_time();
  UBaseType_t count = uxTaskGetSystemState(tasks, MAX_TASKS, &total_runtime);

  const bool have_delta = this->have_previous_tasks_ &&
                          total_runtime >= this->previous_total_runtime_ &&
                          now_us > this->previous_task_sample_us_;
  const uint32_t total_delta = have_delta
      ? total_runtime - this->previous_total_runtime_
      : 0;
  const uint32_t wall_delta_us = have_delta
      ? static_cast<uint32_t>(std::min<int64_t>(
            now_us - this->previous_task_sample_us_,
            static_cast<int64_t>(UINT32_MAX)))
      : 0;

  struct TaskView {
    UBaseType_t index;
    uint32_t delta;
    uint32_t pct_x10;
  };
  TaskView views[MAX_TASKS];
  uint32_t idle_delta[2] = {0, 0};
  uint32_t ready_count[2] = {0, 0};
  uint32_t highest_ready_prio[2] = {0, 0};
  const char *highest_ready_name[2] = {"", ""};
  uint32_t ready_any_count = 0;
  uint32_t highest_ready_any_prio = 0;
  const char *highest_ready_any_name = "";

  for (UBaseType_t i = 0; i < count; i++) {
    uint32_t previous_runtime = 0;
    bool found_previous = false;
    for (const auto &previous : this->previous_tasks_) {
      if (previous.used && previous.task_number == tasks[i].xTaskNumber) {
        previous_runtime = previous.runtime;
        found_previous = true;
        break;
      }
    }
    uint32_t delta = 0;
    if (found_previous && tasks[i].ulRunTimeCounter >= previous_runtime) {
      delta = tasks[i].ulRunTimeCounter - previous_runtime;
    }
    const uint32_t pct_x10 = wall_delta_us > 0
        ? static_cast<uint32_t>((static_cast<uint64_t>(delta) * 1000ULL +
                                 (wall_delta_us / 2)) /
                                wall_delta_us)
        : 0;
    views[i] = TaskView{i, delta, pct_x10};

#if configTASKLIST_INCLUDE_COREID
    const BaseType_t core_id = tasks[i].xCoreID;
    if (core_id == 0 || core_id == 1) {
      const auto core = static_cast<size_t>(core_id);
      const char *name = tasks[i].pcTaskName != nullptr ? tasks[i].pcTaskName : "";
      if ((strcmp(name, "IDLE0") == 0 && core == 0) ||
          (strcmp(name, "IDLE1") == 0 && core == 1)) {
        idle_delta[core] = delta;
      }
      if (tasks[i].eCurrentState == eReady) {
        ready_count[core]++;
        if (tasks[i].uxCurrentPriority >= highest_ready_prio[core]) {
          highest_ready_prio[core] = tasks[i].uxCurrentPriority;
          highest_ready_name[core] = name;
        }
      }
    } else if (core_id == tskNO_AFFINITY && tasks[i].eCurrentState == eReady) {
      const char *name = tasks[i].pcTaskName != nullptr ? tasks[i].pcTaskName : "";
      ready_any_count++;
      if (tasks[i].uxCurrentPriority >= highest_ready_any_prio) {
        highest_ready_any_prio = tasks[i].uxCurrentPriority;
        highest_ready_any_name = name;
      }
    }
#endif
  }

  std::sort(views, views + count, [](const TaskView &a, const TaskView &b) {
    if (a.delta != b.delta) return a.delta > b.delta;
    return a.index < b.index;
  });

  const uint32_t stats_x10 = wall_delta_us > 0
      ? static_cast<uint32_t>((static_cast<uint64_t>(total_delta) * 1000ULL +
                               (wall_delta_us / 2)) /
                              wall_delta_us)
      : 0;
  ESP_LOGI(TAG, "tasks: count=%u total_runtime=%u delta=%u wall_us=%u stats=%u.%u%% first=%s",
           (unsigned) count, (unsigned) total_runtime,
           (unsigned) total_delta, (unsigned) wall_delta_us,
           (unsigned) (stats_x10 / 10), (unsigned) (stats_x10 % 10),
           have_delta ? "no" : "yes");
#if configTASKLIST_INCLUDE_COREID
  if (wall_delta_us > 0) {
    const uint32_t core_capacity = std::max<uint32_t>(1, wall_delta_us);
    for (size_t core = 0; core < 2; core++) {
      const uint32_t idle = std::min<uint32_t>(idle_delta[core], core_capacity);
      const uint32_t busy = core_capacity - idle;
      const uint32_t busy_x10 =
          static_cast<uint32_t>((static_cast<uint64_t>(busy) * 1000ULL +
                                 (core_capacity / 2)) /
                                core_capacity);
      const uint32_t idle_x10 =
          static_cast<uint32_t>((static_cast<uint64_t>(idle) * 1000ULL +
                                 (core_capacity / 2)) /
                                core_capacity);
      ESP_LOGI(TAG,
               "core: id=%u busy=%u.%u%% idle=%u.%u%% idle_delta=%u ready=%u top_ready_prio=%u top_ready=%s",
               (unsigned) core,
               (unsigned) (busy_x10 / 10), (unsigned) (busy_x10 % 10),
               (unsigned) (idle_x10 / 10), (unsigned) (idle_x10 % 10),
               (unsigned) idle_delta[core], (unsigned) ready_count[core],
               (unsigned) highest_ready_prio[core], highest_ready_name[core]);
    }
    ESP_LOGI(TAG, "core: any_ready=%u top_any_ready_prio=%u top_any_ready=%s",
             (unsigned) ready_any_count,
             (unsigned) highest_ready_any_prio,
             highest_ready_any_name);
  }
#endif
  const UBaseType_t logged_count =
      std::min<UBaseType_t>(count, static_cast<UBaseType_t>(this->task_log_limit_));
  for (UBaseType_t rank = 0; rank < logged_count; rank++) {
    const auto &view = views[rank];
    const auto &t = tasks[view.index];
    char core_buf[8];
#if configTASKLIST_INCLUDE_COREID
    const char *core = task_core_to_str(t.xCoreID, core_buf, sizeof(core_buf));
#else
    const char *core = "?";
#endif
    ESP_LOGI(TAG,
             "task: rank=%u name=%s state=%s core=%s prio=%u base=%u stack_hwm_b=%u stack_region=%s runtime=%u delta=%u cpu=%u.%u%%",
             (unsigned) rank,
             t.pcTaskName != nullptr ? t.pcTaskName : "(null)",
             task_state_to_str(t.eCurrentState),
             core,
             (unsigned) t.uxCurrentPriority,
             (unsigned) t.uxBasePriority,
             (unsigned) (t.usStackHighWaterMark * sizeof(StackType_t)),
             stack_region_to_str(t.pxStackBase),
             (unsigned) t.ulRunTimeCounter,
             (unsigned) view.delta,
             (unsigned) (view.pct_x10 / 10),
             (unsigned) (view.pct_x10 % 10));
  }

  memset(this->previous_tasks_, 0, sizeof(this->previous_tasks_));
  for (UBaseType_t i = 0; i < count && i < MAX_TASKS; i++) {
    this->previous_tasks_[i].task_number = static_cast<uint32_t>(tasks[i].xTaskNumber);
    this->previous_tasks_[i].runtime = static_cast<uint32_t>(tasks[i].ulRunTimeCounter);
    this->previous_tasks_[i].used = true;
  }
  this->previous_total_runtime_ = total_runtime;
  this->previous_task_sample_us_ = now_us;
  this->have_previous_tasks_ = true;
#else
  ESP_LOGW(TAG, "tasks: unavailable; enable CONFIG_FREERTOS_USE_TRACE_FACILITY");
#endif
}

void RuntimeDiag::dump_task_backtraces_() {
#if CONFIG_IDF_TARGET_ARCH_XTENSA
  ESP_LOGI(TAG, "backtrace: all tasks begin");
  esp_err_t err = esp_backtrace_print_all_tasks(12);
  ESP_LOGI(TAG, "backtrace: all tasks end (%s)", esp_err_to_name(err));
#else
  ESP_LOGW(TAG, "backtrace: all-task helper unavailable on this target; use JTAG/GDB for saved task stacks");
#endif
}

void RuntimeDiag::start_loop_watch_() {
  if (this->loop_watch_task_handle_ != nullptr) return;
  BaseType_t rc = xTaskCreate(&RuntimeDiag::loop_watch_task_, "runtime_diag",
                              this->loop_watch_stack_bytes_, this,
                              static_cast<UBaseType_t>(this->loop_watch_priority_),
                              &this->loop_watch_task_handle_);
  if (rc != pdPASS || this->loop_watch_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "failed to start loop watchdog diagnostic task");
    this->loop_watch_task_handle_ = nullptr;
    return;
  }
  ESP_LOGI(TAG, "loop watchdog diagnostic task started (interval=%ums threshold=%ums priority=%u stack=%uB)",
           (unsigned) this->loop_watch_interval_ms_,
           (unsigned) this->loop_stall_threshold_ms_,
           (unsigned) this->loop_watch_priority_,
           (unsigned) this->loop_watch_stack_bytes_);
}

void RuntimeDiag::loop_watch_task_(void *arg) {
  static_cast<RuntimeDiag *>(arg)->loop_watch_();
}

void RuntimeDiag::loop_watch_() {
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(this->loop_watch_interval_ms_));
    this->loop_watch_tick_.store(xTaskGetTickCount(), std::memory_order_relaxed);
    this->loop_watch_iterations_.fetch_add(1, std::memory_order_relaxed);
    const uint32_t now = millis();
    const uint32_t heartbeat = this->loop_heartbeat_ms_.load(std::memory_order_relaxed);
    const uint32_t age = now - heartbeat;
    if (age < this->loop_stall_threshold_ms_) continue;

    const uint32_t last_dump = this->last_loop_stall_dump_ms_.load(std::memory_order_relaxed);
    if (now - last_dump < this->loop_stall_threshold_ms_) continue;

    this->last_loop_stall_dump_ms_.store(now, std::memory_order_relaxed);
    ESP_LOGE(TAG, "loopTask heartbeat stale for %ums; dumping runtime snapshot before TWDT",
             (unsigned) age);
    this->dump_("loop_stall");
  }
}

void RuntimeDiag::print_twdt_isr() const {
  const uint32_t now_tick = xTaskGetTickCountFromISR();
  const uint32_t hb_tick = this->loop_heartbeat_tick_.load(std::memory_order_relaxed);
  const uint32_t watch_tick = this->loop_watch_tick_.load(std::memory_order_relaxed);
  const uint32_t iterations = this->loop_watch_iterations_.load(std::memory_order_relaxed);
  ESP_EARLY_LOGE(TAG,
                 "twdt_isr: loop_hb_age=%ums watcher_age=%ums watcher_iter=%u",
                 (unsigned) ((now_tick - hb_tick) * portTICK_PERIOD_MS),
                 (unsigned) ((now_tick - watch_tick) * portTICK_PERIOD_MS),
                 (unsigned) iterations);
}

}  // namespace runtime_diag
}  // namespace esphome

extern "C" void esp_task_wdt_isr_user_handler(void) {
  auto *diag = esphome::runtime_diag::global_runtime_diag;
  if (diag == nullptr) return;
  diag->print_twdt_isr();
}
