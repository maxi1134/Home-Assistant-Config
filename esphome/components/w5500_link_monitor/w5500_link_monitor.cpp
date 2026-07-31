#include "w5500_link_monitor.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif

namespace esphome::w5500_link_monitor {

// W5500 SPI frame: 16-bit register address (command phase), then 8-bit control
// byte (address phase). Control byte = BSB[7:3] | RWB[2] | OM[1:0]. For a read
// of the common register block in VDM mode, every field is zero -> control byte
// 0x00.
static constexpr uint16_t W5500_PHYCFGR_ADDR = 0x002E;
static constexpr uint8_t W5500_CONTROL_READ_COMMON = 0x00;
static constexpr uint8_t W5500_PHYCFGR_LNK_BIT = 0x01;

static const char *const TAG = "w5500_link_monitor";

void W5500LinkMonitor::setup() {
  // Bring the W5500 out of reset so PHYCFGR will read a valid link state once
  // start() initializes SPI. Safe on ethernet-mode boots too: the ethernet
  // driver re-drives this pin during its own init via phy_config.reset_gpio_num
  // and gpio_config is idempotent.
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = 1ULL << this->reset_pin_;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
  gpio_set_level(static_cast<gpio_num_t>(this->reset_pin_), 1);

  // PollingComponent::call_setup() arms the poller before invoking setup(). We
  // don't want polling until start() is called (on ethernet-mode boots, start()
  // is never called and we must stay quiet so we don't fight the ethernet
  // component for the bus).
  this->stop_poller();
}

void W5500LinkMonitor::start() {
  if (this->active_) {
    ESP_LOGW(TAG, "start() called twice; ignoring");
    return;
  }
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr &&
      ethernet::global_eth_component->is_enabled()) {
    ESP_LOGE(TAG, "Start refused: ethernet component is already enabled");
    return;
  }
#endif

  ESP_LOGD(TAG, "Initializing SPI for link polling");

  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = this->mosi_pin_;
  buscfg.miso_io_num = this->miso_pin_;
  buscfg.sclk_io_num = this->clk_pin_;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = 0;
  esp_err_t err =
      spi_bus_initialize(this->interface_, &buscfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  spi_device_interface_config_t devcfg = {};
  devcfg.command_bits = 16; // W5500 register address phase
  devcfg.address_bits = 8;  // W5500 control byte phase
  devcfg.mode = 0;
  devcfg.clock_speed_hz = static_cast<int>(this->clock_speed_);
  devcfg.spics_io_num = this->cs_pin_;
  devcfg.queue_size = 1;
  err = spi_bus_add_device(this->interface_, &devcfg, &this->spi_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
    spi_bus_free(this->interface_);
    this->mark_failed();
    return;
  }

  this->active_ = true;
  this->last_link_up_ = false;
  this->start_poller();
}

bool W5500LinkMonitor::read_phycfgr_(uint8_t *out) {
  // len <= 4 -> use the inline SPI_TRANS_USE_RXDATA buffer to avoid a
  // per-transaction DMA-capable allocation, same guard the stock W5500 driver
  // uses for small reads.
  spi_transaction_t trans = {};
  trans.flags = SPI_TRANS_USE_RXDATA;
  trans.cmd = W5500_PHYCFGR_ADDR;
  trans.addr = W5500_CONTROL_READ_COMMON;
  trans.length = 8;
  esp_err_t err = spi_device_polling_transmit(this->spi_handle_, &trans);
  if (err != ESP_OK) {
    return false;
  }
  *out = trans.rx_data[0];
  return true;
}

void W5500LinkMonitor::update() {
  if (!this->active_) {
    return;
  }
  uint8_t phycfgr;
  if (!this->read_phycfgr_(&phycfgr)) {
    ESP_LOGW(TAG, "Failed to read PHYCFGR");
    return;
  }
  const bool link_up = (phycfgr & W5500_PHYCFGR_LNK_BIT) != 0;
  if (link_up && !this->last_link_up_) {
    ESP_LOGD(TAG, "Link detected (PHYCFGR=0x%02X), firing on_link_up", phycfgr);
    // Disarm and tear down the SPI bus before invoking the callback. The
    // callback is expected to reboot, but if the user's chain defers (delay,
    // wait_until) or omits the restart during testing, the bus must be
    // releasable — otherwise ethernet's enable() on the next boot path can't
    // claim the host.
    this->active_ = false;
    this->stop_poller();
    if (this->spi_handle_ != nullptr) {
      spi_bus_remove_device(this->spi_handle_);
      this->spi_handle_ = nullptr;
    }
    spi_bus_free(this->interface_);
    this->link_up_callback_.call();
    return;
  }
  this->last_link_up_ = link_up;
}

void W5500LinkMonitor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "W5500 Link Monitor:\n"
                "  Interface: spi%d\n"
                "  Clock Speed: %u MHz\n"
                "  CLK Pin: GPIO%u\n"
                "  MISO Pin: GPIO%u\n"
                "  MOSI Pin: GPIO%u\n"
                "  CS Pin: GPIO%u\n"
                "  Reset Pin: GPIO%u",
                this->interface_ == SPI3_HOST ? 3 : 2,
                this->clock_speed_ / 1000000, this->clk_pin_, this->miso_pin_,
                this->mosi_pin_, this->cs_pin_, this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

} // namespace esphome::w5500_link_monitor

#endif // USE_ESP32
