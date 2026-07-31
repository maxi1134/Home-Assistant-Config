#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32
#include <driver/gpio.h>
#include <driver/spi_master.h>

namespace esphome::w5500_link_monitor {

// Polls the W5500's PHYCFGR register over SPI on WiFi-mode boots and invokes
// the on_link_up callback when an ethernet cable is detected, so the YAML can
// flip use_wifi and reboot into ethernet mode. Mutually exclusive with the
// ethernet component per boot — only one of the two ever calls
// spi_bus_initialize() on the shared host.
class W5500LinkMonitor : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI; }

  // Activates the monitor: takes ownership of the SPI bus, adds our device,
  // and arms the poller. Called from the w5500_link_monitor.start action only
  // on WiFi-mode boots. No-op (with warning) if ethernet is already enabled —
  // belt-and-braces against the boot lambda somehow activating both.
  void start();

  void set_interface(spi_host_device_t interface) { this->interface_ = interface; }
  void set_clock_speed(uint32_t clock_speed) { this->clock_speed_ = clock_speed; }
  void set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }
  void set_miso_pin(uint8_t pin) { this->miso_pin_ = pin; }
  void set_mosi_pin(uint8_t pin) { this->mosi_pin_ = pin; }
  void set_cs_pin(uint8_t pin) { this->cs_pin_ = pin; }
  void set_reset_pin(uint8_t pin) { this->reset_pin_ = pin; }

  // Templated so YAML-generated lightweight forwarder structs (from
  // build_callback_automation) bind without heap-allocating an std::function.
  template<typename F> void add_on_link_up_callback(F &&callback) {
    this->link_up_callback_.add(std::forward<F>(callback));
  }

 protected:
  bool read_phycfgr_(uint8_t *out);

  spi_host_device_t interface_{SPI2_HOST};
  uint32_t clock_speed_{20'000'000};
  uint8_t clk_pin_{0};
  uint8_t miso_pin_{0};
  uint8_t mosi_pin_{0};
  uint8_t cs_pin_{0};
  uint8_t reset_pin_{0};

  spi_device_handle_t spi_handle_{nullptr};
  bool active_{false};
  bool last_link_up_{false};
  // Lazy variant — most users won't wire on_link_up to anything, so the empty
  // case costs 4 bytes (nullptr) instead of 12 (std::vector header).
  LazyCallbackManager<void()> link_up_callback_;
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<W5500LinkMonitor> {
 public:
  void play(Ts... /*x*/) override { this->parent_->start(); }
};

}  // namespace esphome::w5500_link_monitor

#endif  // USE_ESP32
