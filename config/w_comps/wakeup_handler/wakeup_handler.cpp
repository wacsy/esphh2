#include "wakeup_handler.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wakeup_handler {

static const char *const TAG = "wakeup_handler";

void WakeupHandler::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Wakeup Handler...");

  // 获取唤醒原因
#ifdef USE_ESP32
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  this->wakeup_cause_ = static_cast<uint8_t>(cause);
#else
  this->wakeup_cause_ = 0;  // ESP8266 不支持唤醒原因检测
#endif

  ESP_LOGD(TAG, "Wakeup cause: %d (%s)", this->wakeup_cause_, this->get_wakeup_cause_string());

  // 执行唤醒回调
  this->execute_wakeup_callbacks_();
}

void WakeupHandler::dump_config() {
  ESP_LOGCONFIG(TAG, "Wakeup Handler:");
  ESP_LOGCONFIG(TAG, "  Wakeup callbacks: %d", this->wakeup_callbacks_.size());
  ESP_LOGCONFIG(TAG, "  Wakeup cause callbacks: %d", this->wakeup_cause_callbacks_.size());
  ESP_LOGCONFIG(TAG, "  Last wakeup cause: %d (%s)", this->wakeup_cause_, this->get_wakeup_cause_string());
}

float WakeupHandler::get_setup_priority() const {
  // 在其他组件之前执行，这样可以先处理唤醒逻辑
  return setup_priority::LATE + 1.0f;
}

void WakeupHandler::execute_wakeup_callbacks_() {
  if (this->callbacks_executed_) {
    return;
  }

  ESP_LOGD(TAG, "Executing wakeup callbacks");

  // 执行通用唤醒回调
  for (auto &callback : this->wakeup_callbacks_) {
    callback();
  }

  // 执行唤醒原因回调
  for (auto &callback : this->wakeup_cause_callbacks_) {
    callback(this->wakeup_cause_);
  }

  this->callbacks_executed_ = true;
}

const char *WakeupHandler::get_wakeup_cause_string() {
#ifdef USE_ESP32
  switch (this->wakeup_cause_) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return "Reset/Power-on";
    case ESP_SLEEP_WAKEUP_EXT0:
      return "External (RTC_IO)";
    case ESP_SLEEP_WAKEUP_EXT1:
      return "External (RTC_CNTL)";
    case ESP_SLEEP_WAKEUP_TIMER:
      return "Timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      return "Touchpad";
    case ESP_SLEEP_WAKEUP_ULP:
      return "ULP";
    default:
      return "Unknown";
  }
#else
  return "Not supported on ESP8266";
#endif
}

}  // namespace wakeup_handler
}  // namespace esphome
