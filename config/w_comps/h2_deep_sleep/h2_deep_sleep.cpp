#include "h2_deep_sleep.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#endif

namespace esphome {
namespace h2_deep_sleep {

static const char *const TAG = "h2_deep_sleep";

void H2DeepSleep::setup() {
  ESP_LOGCONFIG(TAG, "Setting up H2 Deep Sleep...");

#ifdef USE_ESP32
  // 获取唤醒原因
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  this->wakeup_cause_ = static_cast<uint8_t>(cause);

  //   ESP_LOGI(TAG, "Wakeup cause: %s", this->get_wakeup_cause_string());

  // 如果是GPIO唤醒，先禁用对应的唤醒IO，避免立即再次唤醒
  if (cause == ESP_SLEEP_WAKEUP_EXT1 && this->wakeup_pin_ != nullptr) {
    uint8_t pin_num = this->wakeup_pin_->get_pin();
    uint64_t pin_mask = (1ULL << pin_num);

    // 禁用该引脚的唤醒功能
    esp_err_t err = esp_sleep_disable_ext1_wakeup_io(pin_mask);
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "Disabled EXT1 wakeup on GPIO%d to prevent immediate re-wakeup", pin_num);
    } else {
      ESP_LOGW(TAG, "Failed to disable EXT1 wakeup IO: %d (this is normal on first boot)", err);
    }

    // 配置为输入以读取电平
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = pin_mask;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    this->wakeup_pin_level_ = this->wakeup_pin_->digital_read() ? 1 : 0;
    ESP_LOGI(TAG, "Wakeup pin GPIO%d level: %d", pin_num, this->wakeup_pin_level_);
  }
#endif

  this->setup_complete_ = true;
}

void H2DeepSleep::loop() {
  if (!this->setup_complete_ || this->entering_deep_sleep_) {
    return;
  }

  if (this->run_duration_.has_value() && !this->sleep_prevented_) {
    uint32_t now = millis();

    if (this->sleep_start_time_ == 0) {
      this->sleep_start_time_ = now;
      ESP_LOGI(TAG, "Run duration timer started: %u ms", this->run_duration_.value());
    }

    uint32_t elapsed = now - this->sleep_start_time_;
    uint32_t target = this->run_duration_.value();

    // 每秒打印一次倒计时
    // static uint32_t last_log_time = 0;
    // if (now - last_log_time >= 1000) {
    //   uint32_t remaining = (target > elapsed) ? (target - elapsed) / 1000 : 0;
    //   if (remaining > 0 && remaining <= 10) {
    //     ESP_LOGI(TAG, "Deep sleep in %u seconds...", remaining);
    //   }
    //   last_log_time = now;
    // }

    if (elapsed >= target) {
      //   ESP_LOGI(TAG, "=== RUN DURATION EXPIRED (%u ms) ===", target);
      ESP_LOGI(TAG, "Entering deep sleep ...");
      this->begin_sleep();
      // 注意：begin_sleep() 会设置 entering_deep_sleep_ = true
      // 下次loop会直接return，不会再次调用
    }
  }
}

void H2DeepSleep::dump_config() {
  //   ESP_LOGCONFIG(TAG, "H2 Deep Sleep:");

  //   if (this->sleep_duration_.has_value()) {
  //     ESP_LOGCONFIG(TAG, "  Sleep Duration: %u ms", this->sleep_duration_.value());
  //   }

  //   if (this->run_duration_.has_value()) {
  //     ESP_LOGCONFIG(TAG, "  Run Duration: %u ms", this->run_duration_.value());
  //   }

  //   if (this->wakeup_pin_.has_value()) {
  //     ESP_LOGCONFIG(TAG, "  Wakeup Pin: GPIO%d", this->wakeup_pin_.value());
  //     ESP_LOGCONFIG(TAG, "  Wakeup Pin Mode: %d", this->wakeup_pin_mode_);
  //   }

  //   ESP_LOGCONFIG(TAG, "  Last Wakeup Cause: %s", this->get_wakeup_cause_string());

  //   if (this->wakeup_cause_ == ESP_SLEEP_WAKEUP_EXT1 && this->wakeup_pin_.has_value()) {
  //     ESP_LOGCONFIG(TAG, "  Wakeup Pin Level: %d", this->wakeup_pin_level_);
  //   }
}

float H2DeepSleep::get_setup_priority() const { return setup_priority::LATE; }
void H2DeepSleep::begin_sleep(uint32_t sleep_duration_ms) {
  if (this->sleep_prevented_) {
    ESP_LOGW(TAG, "Deep sleep prevented, not entering sleep mode");
    return;
  }

  // 防止重复调用
  if (this->entering_deep_sleep_) {
    ESP_LOGD(TAG, "Already entering deep sleep, ignoring duplicate call");
    return;
  }

  this->entering_deep_sleep_ = true;  // 设置标志

  uint32_t duration = sleep_duration_ms > 0 ? sleep_duration_ms : this->sleep_duration_.value_or(0);

  ESP_LOGI(TAG, "Beginning deep sleep preparation...");

  // 先配置唤醒源
  this->prepare_deep_sleep(duration);

  // 创建 deep sleep task
  ESP_LOGI(TAG, "Creating deep sleep task...");
  xTaskCreate(this->deep_sleep_task, "deep_sleep_task", 4096, this, 5, &this->deep_sleep_task_handle_);
}

void H2DeepSleep::deep_sleep_task(void *param) {
  H2DeepSleep *self = static_cast<H2DeepSleep *>(param);

  //   ESP_LOGI(TAG, "Deep sleep task started, waiting for other tasks...");

  // 给其他任务一点时间完成
  vTaskDelay(pdMS_TO_TICKS(200));

  //   ESP_LOGI(TAG, "");
  //   ESP_LOGI(TAG, "╔════════════════════════════════════╗");
  ESP_LOGI(TAG, "║  ENTERING DEEP SLEEP MODE NOW...  ║");
  //   ESP_LOGI(TAG, "╚════════════════════════════════════╝");
  //   ESP_LOGI(TAG, "");

  // 可选：通知App停止循环（如果需要）
  // App.schedule_dump_config();

  //   ESP_LOGI(TAG, "All tasks completed, entering deep sleep NOW!");

  // 刷新日志
  vTaskDelay(pdMS_TO_TICKS(100));

  // 进入 deep sleep
  esp_deep_sleep_start();

  // 不应该执行到这里
  ESP_LOGE(TAG, "ERROR: Should not reach here after deep sleep!");
  vTaskDelete(NULL);
}

void H2DeepSleep::prepare_deep_sleep(uint32_t sleep_duration_ms) {
#ifdef USE_ESP32
  ESP_LOGI(TAG, "Configuring deep sleep wakeup sources...");

  // 配置定时器唤醒
  if (sleep_duration_ms > 0) {
    uint64_t sleep_duration_us = sleep_duration_ms * 1000ULL;
    esp_sleep_enable_timer_wakeup(sleep_duration_us);
    ESP_LOGI(TAG, "  ✓ Timer wakeup: %u ms (%llu us)", sleep_duration_ms, sleep_duration_us);
  } else {
    ESP_LOGW(TAG, "  ✗ No timer wakeup configured (duration = 0)");
  }

  // 配置 EXT1 GPIO 唤醒
  this->configure_ext1_wakeup();

  ESP_LOGI(TAG, "Deep sleep configuration complete");
#endif
}

void H2DeepSleep::configure_ext1_wakeup() {
#ifdef USE_ESP32
  if (this->wakeup_pin_ == nullptr) {
    return;
  }

  uint8_t pin_num = this->wakeup_pin_->get_pin();  // ← 获取引脚号
  uint64_t pin_mask = (1ULL << pin_num);

  ESP_LOGI(TAG, "Configuring EXT1 wakeup on GPIO%d", pin_num);

  // 设置为输入
  this->wakeup_pin_->setup();
  this->wakeup_pin_->pin_mode(gpio::FLAG_INPUT);

  // 读取当前电平
  bool current_level_bool = this->wakeup_pin_->digital_read();
  int current_level = current_level_bool ? 1 : 0;

  ESP_LOGI(TAG, "Current GPIO%d level: %d", pin_num, current_level);

  // 根据 wakeup_pin_mode 和当前电平决定唤醒模式
  esp_sleep_ext1_wakeup_mode_t wakeup_mode;

  switch (this->wakeup_pin_mode_) {
    case WAKEUP_PIN_MODE_IGNORE:
      ESP_LOGI(TAG, "Wakeup pin mode set to IGNORE, not configuring EXT1");
      return;

    case WAKEUP_PIN_MODE_KEEP_AWAKE:
      // 如果引脚已经是高电平，不进入睡眠
      if (current_level == 1) {
        ESP_LOGW(TAG, "Wakeup pin is HIGH, preventing sleep");
        this->prevent_deep_sleep();
        return;
      }
      wakeup_mode = ESP_EXT1_WAKEUP_ANY_HIGH;
      break;

    case WAKEUP_PIN_MODE_INVERT_WAKEUP:
      // 根据当前电平反向设置唤醒条件
      if (current_level == 0) {
        wakeup_mode = ESP_EXT1_WAKEUP_ANY_HIGH;
        ESP_LOGI(TAG, "Pin is LOW, will wake on HIGH");
      } else {
        wakeup_mode = ESP_EXT1_WAKEUP_ANY_LOW;
        ESP_LOGI(TAG, "Pin is HIGH, will wake on LOW");
      }
      break;

    default:
      wakeup_mode = ESP_EXT1_WAKEUP_ANY_HIGH;
      break;
  }

  // 直接配置 EXT1 唤醒（不需要先禁用）
  // esp_sleep_enable_ext1_wakeup 会自动覆盖之前的配置
  esp_err_t err = esp_sleep_enable_ext1_wakeup_io(pin_mask, wakeup_mode);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "  ✓ EXT1 wakeup enabled on GPIO%d (mode: %s)", pin_num,
             wakeup_mode == ESP_EXT1_WAKEUP_ANY_HIGH ? "ANY_HIGH" : "ALL_LOW");
  } else {
    ESP_LOGE(TAG, "  ✗ Failed to enable EXT1 wakeup: %d", err);
  }
#endif
}

void H2DeepSleep::prevent_deep_sleep() {
  ESP_LOGI(TAG, "Deep sleep prevented");
  this->sleep_prevented_ = true;
}

void H2DeepSleep::allow_deep_sleep() {
  ESP_LOGI(TAG, "Deep sleep allowed");
  this->sleep_prevented_ = false;
}

uint8_t H2DeepSleep::get_wakeup_cause() { return this->wakeup_cause_; }

const char *H2DeepSleep::get_wakeup_cause_string() {
#ifdef USE_ESP32
  switch (this->wakeup_cause_) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return "Reset/Power-on";
    case ESP_SLEEP_WAKEUP_EXT0:
      return "External (RTC_IO)";
    case ESP_SLEEP_WAKEUP_EXT1:
      return "External (RTC_CNTL/GPIO)";
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
  return "Not ESP32";
#endif
}

uint8_t H2DeepSleep::get_wakeup_pin_level() { return this->wakeup_pin_level_; }

}  // namespace h2_deep_sleep
}  // namespace esphome
