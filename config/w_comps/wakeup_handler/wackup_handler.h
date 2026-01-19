#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include <vector>
#include <functional>

#ifdef USE_ESP32
#include <esp_sleep.h>
#endif

namespace esphome {
namespace wakeup_handler {

class WakeupHandler : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  // 添加唤醒回调
  void add_on_wakeup_callback(std::function<void()> &&callback) { this->wakeup_callbacks_.push_back(callback); }

  // 添加唤醒原因回调
  void add_on_wakeup_cause_callback(std::function<void(uint8_t)> &&callback) {
    this->wakeup_cause_callbacks_.push_back(callback);
  }

  // 获取唤醒原因
  uint8_t get_wakeup_cause() { return this->wakeup_cause_; }

  // 获取唤醒原因的字符串描述
  const char *get_wakeup_cause_string();

 protected:
  void execute_wakeup_callbacks_();

  std::vector<std::function<void()>> wakeup_callbacks_;
  std::vector<std::function<void(uint8_t)>> wakeup_cause_callbacks_;
  uint8_t wakeup_cause_{0};
  bool callbacks_executed_{false};
};

// 通用唤醒触发器
class WakeupTrigger : public Trigger<> {
 public:
  explicit WakeupTrigger(WakeupHandler *parent) {
    parent->add_on_wakeup_callback([this]() { this->trigger(); });
  }
};

// 带唤醒原因的触发器
class WakeupCauseTrigger : public Trigger<uint8_t> {
 public:
  explicit WakeupCauseTrigger(WakeupHandler *parent) {
    parent->add_on_wakeup_cause_callback([this](uint8_t cause) { this->trigger(cause); });
  }
};

}  // namespace wakeup_handler
}  // namespace esphome
