#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

#ifdef USE_ESP32
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace esphome {
namespace h2_deep_sleep {

enum WakeupPinMode {
  WAKEUP_PIN_MODE_IGNORE = 0,
  WAKEUP_PIN_MODE_KEEP_AWAKE,
  WAKEUP_PIN_MODE_INVERT_WAKEUP,
};

class H2DeepSleep : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  // 配置方法
  void set_sleep_duration(uint32_t time_ms) { this->sleep_duration_ = time_ms; }
  void set_run_duration(uint32_t time_ms) { this->run_duration_ = time_ms; }
  void set_wakeup_pin(InternalGPIOPin *pin) { this->wakeup_pin_ = pin; }
  void set_wakeup_pin_mode(WakeupPinMode mode) { this->wakeup_pin_mode_ = mode; }

  // 控制方法
  void begin_sleep(uint32_t sleep_duration_ms = 0);
  void prevent_deep_sleep();
  void allow_deep_sleep();

  // 获取唤醒信息
  uint8_t get_wakeup_cause();
  const char *get_wakeup_cause_string();
  uint8_t get_wakeup_pin_level();

 protected:
  // Deep Sleep Task
  static void deep_sleep_task(void *param);
  void prepare_deep_sleep(uint32_t sleep_duration_ms);
  void configure_ext1_wakeup();

  optional<uint32_t> sleep_duration_{};
  optional<uint32_t> run_duration_{};
  InternalGPIOPin *wakeup_pin_{nullptr};
  WakeupPinMode wakeup_pin_mode_{WAKEUP_PIN_MODE_INVERT_WAKEUP};

  uint32_t sleep_start_time_{0};
  uint8_t wakeup_cause_{0};
  uint8_t wakeup_pin_level_{0};
  bool sleep_prevented_{false};
  bool setup_complete_{false};
  bool entering_deep_sleep_{false};

  TaskHandle_t deep_sleep_task_handle_{nullptr};
};

template<typename... Ts> class EnterDeepSleepAction : public Action<Ts...>, public Parented<H2DeepSleep> {
 public:
  TEMPLATABLE_VALUE(uint32_t, sleep_duration)

  void play(Ts... x) override {
    if (this->sleep_duration_.has_value()) {
      this->parent_->begin_sleep(this->sleep_duration_.value(x...));
    } else {
      this->parent_->begin_sleep();
    }
  }
};

template<typename... Ts> class PreventDeepSleepAction : public Action<Ts...>, public Parented<H2DeepSleep> {
 public:
  void play(Ts... x) override { this->parent_->prevent_deep_sleep(); }
};

template<typename... Ts> class AllowDeepSleepAction : public Action<Ts...>, public Parented<H2DeepSleep> {
 public:
  void play(Ts... x) override { this->parent_->allow_deep_sleep(); }
};

}  // namespace h2_deep_sleep
}  // namespace esphome
