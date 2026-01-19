

# ESP32-H2 Deep Sleep Component

一个专为 ESP32-H2 设计的 ESPHome 自定义 Deep Sleep 组件，支持定时唤醒、GPIO 唤醒，以及智能电平反转唤醒机制。
A production‑grade Deep Sleep controller for ESPHome, purpose‑built for the ESP32‑H2 platform.
It provides precise wakeup control, GPIO EXT1 wakeup, smart inverted‑level logic, and seamless integration with other ESPHome components — ideal for ultra‑low‑power sensor nodes and battery‑powered devices.

## 📋 目录

- [功能特性](#功能特性)
- [开发背景](#开发背景)
- [文件结构](#文件结构)
- [安装方法](#安装方法)
- [配置说明](#配置说明)
- [使用示例](#使用示例)
- [开发过程](#开发过程)
- [常见问题](#常见问题)
- [API 参考](#api-参考)

## ✨ 功能特性

- ✅ **定时唤醒** - 支持配置睡眠时长，自动定时唤醒 Timer‑based wakeup with configurable sleep duration
- ✅ **GPIO EXT1 唤醒** - 支持单个 GPIO 引脚作为唤醒源 GPIO EXT1 wakeup with full Pin Schema support
- ✅ **智能电平反转** - 自动读取 GPIO 电平，设置相反的唤醒条件，避免立即再次唤醒 Smart inverted‑level wakeup to avoid immediate re‑trigger
- ✅ **运行时长控制** - 设置唤醒后运行时间，自动进入睡眠 Run‑duration control for automatic sleep entry
- ✅ **唤醒原因检测** - 获取唤醒原因（定时器、GPIO、重置等）方便执行不同的初始化逻辑 Wakeup reason detection with human‑readable descriptions
- ✅ **FreeRTOS Task** - 使用独立任务处理睡眠流程，等待其他任务完成 FreeRTOS task‑based sleep handling for safe transitions
- ✅ **引脚共享** - 支持 `allow_other_uses`，允许多个组件使用同一 GPIO Pin sharing support (allow_other_uses)
- ✅ **手动控制** - 提供 Actions 用于手动触发睡眠、阻止睡眠等 Manual sleep control actions (enter, prevent, allow)

Designed specifically for ESP32‑H2 (ESP‑IDF)

## 🎯 开发背景

### 为什么需要这个组件？

ESPHome 内置的 `deep_sleep` 组件主要针对 ESP32 和 ESP8266，对于 ESP32-H2 这样的新芯片，需要更精细的控制。

### 开发过程中遇到的问题

#### 问题 1: 头文件找不到
```
fatal error: esphome/components/deep_sleep/deep_sleep_component.h: No such file or directory
```
**解决方案**: 放弃继承 `deep_sleep::DeepSleepComponent`，创建独立组件。

#### 问题 2: 禁用唤醒源失败
```
E (10767) sleep: Incorrect wakeup source (3) to disable.
```
**解决方案**:
- 移除 `esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1)`
- 改用 `esp_sleep_disable_ext1_wakeup_io(pin_mask)` 禁用特定引脚
- 使用 `esp_sleep_enable_ext1_wakeup_io()` 替代 `esp_sleep_enable_ext1_wakeup()`

#### 问题 3: 重复进入睡眠流程
```
[20:57:46][I][h2_deep_sleep:083]: === RUN DURATION EXPIRED (10000 ms) ===
[20:57:46][I][h2_deep_sleep:084]: Entering deep sleep automatically...
... (重复多次)
```
**解决方案**: 添加 `entering_deep_sleep_` 标志位，防止 `loop()` 函数重复调用 `begin_sleep()`。

#### 问题 4: 引脚共享支持
```yaml
# 需要支持这种配置
wakeup_pin:
  number: GPIO10
  allow_other_uses: true
```
**解决方案**:
- Python: `pins.internal_gpio_input_pin_number` → `pins.internal_gpio_input_pin_schema`
- Python: 使用 `await cg.gpio_pin_expression(config[CONF_WAKEUP_PIN])`
- C++: `uint8_t wakeup_pin_` → `InternalGPIOPin *wakeup_pin_`

## 📁 文件结构

```
w_comps/
└── h2_deep_sleep/
    ├── __init__.py           # Python 配置验证和代码生成
    ├── h2_deep_sleep.h       # C++ 头文件
    └── h2_deep_sleep.cpp     # C++ 实现
```

## 🚀 安装方法 Installation

Local Installation
1. 在你的 ESPHome 配置目录下创建 `w_comps` 文件夹
2. 将 `h2_deep_sleep` 文件夹复制到 `w_comps` 中
3. 在 YAML 中添加：

```yaml
external_components:
  - source:
      type: local
      path: w_comps
    components: [h2_deep_sleep]
```

## ⚙️ 配置说明 Configuration

### 基本配置 Basic Example

```yaml
h2_deep_sleep:
  id: deep_sleep_ctrl                # 组件 ID（必需）
  run_duration: 30s                  # 唤醒后运行时长（可选）
  sleep_duration: 5min               # 睡眠时长（可选）
  wakeup_pin:                        # 唤醒引脚（可选）
    number: GPIO10
    allow_other_uses: true
  wakeup_pin_mode: INVERT_WAKEUP     # 唤醒模式（可选）
```

### 参数说明 parameters

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | ID | - | 组件标识符（必需） |
| `run_duration` | Time | - | 唤醒后运行时长，超时自动睡眠 |
| `sleep_duration` | Time | - | Deep Sleep 睡眠时长 |
| `wakeup_pin` | Pin Schema | - | EXT1 唤醒引脚配置 |
| `wakeup_pin_mode` | Enum | `INVERT_WAKEUP` | 唤醒引脚模式 |

### 唤醒模式 (wakeup_pin_mode)

| 模式 | 说明 |
|------|------|
| `IGNORE` | 忽略唤醒引脚，不配置 EXT1 唤醒 |
| `KEEP_AWAKE` | 如果引脚为高电平，阻止进入睡眠 |
| `INVERT_WAKEUP` | **智能反转** - 读取当前电平，设置相反的唤醒条件（推荐） |

## 📝 使用示例 Examples

### 示例 1: 基础定时唤醒

```yaml
esp32:
  board: esp32-h2-devkitm-1
  variant: esp32h2
  framework:
    type: esp-idf

logger:
  level: DEBUG

h2_deep_sleep:
  id: deep_sleep_ctrl
  run_duration: 20s      # 唤醒后运行 20 秒
  sleep_duration: 5min   # 睡眠 5 分钟
```

### 示例 2: GPIO 唤醒 + 定时唤醒

```yaml
h2_deep_sleep:
  id: deep_sleep_ctrl
  run_duration: 30s
  sleep_duration: 10min
  wakeup_pin:
    number: GPIO10
    allow_other_uses: true
  wakeup_pin_mode: INVERT_WAKEUP

# 可以同时使用 GPIO10 作为按钮
binary_sensor:
  - platform: gpio
    name: "Wake Button"
    pin:
      number: GPIO10
      mode:
        input: true
      allow_other_uses: true
    on_press:
      - logger.log: "Button pressed!"
```

### 示例 3: 根据唤醒原因执行不同逻辑

```yaml
h2_deep_sleep:
  id: deep_sleep_ctrl
  run_duration: 20s
  sleep_duration: 5min
  wakeup_pin:
    number: GPIO10
    allow_other_uses: true

esphome:
  name: h2-smart-device
  on_boot:
    priority: -100
    then:
      - lambda: |-
          uint8_t cause = id(deep_sleep_ctrl).get_wakeup_cause();
          ESP_LOGI("main", "Wakeup cause: %s",
                   id(deep_sleep_ctrl).get_wakeup_cause_string());

          switch(cause) {
            case 0:  // ESP_SLEEP_WAKEUP_UNDEFINED - 首次启动
              ESP_LOGI("main", "First boot - full initialization");
              // 执行完整初始化
              break;

            case 4:  // ESP_SLEEP_WAKEUP_TIMER - 定时唤醒
              ESP_LOGI("main", "Timer wakeup - quick sensor read");
              // 快速读取传感器
              break;

            case 3:  // ESP_SLEEP_WAKEUP_EXT1 - GPIO 唤醒
              ESP_LOGI("main", "GPIO wakeup - handle event");
              uint8_t level = id(deep_sleep_ctrl).get_wakeup_pin_level();
              ESP_LOGI("main", "Wakeup pin was at level: %d", level);
              // 处理按钮事件
              break;
          }
```

### 示例 4: 手动控制睡眠

```yaml
h2_deep_sleep:
  id: deep_sleep_ctrl
  sleep_duration: 1min

button:
  - platform: template
    name: "Sleep Now"
    on_press:
      # 立即进入睡眠，覆盖默认睡眠时长
      - h2_deep_sleep.enter:
          id: deep_sleep_ctrl
          sleep_duration: 30s

  - platform: template
    name: "Prevent Sleep"
    on_press:
      - h2_deep_sleep.prevent:
          id: deep_sleep_ctrl
      - logger.log: "Sleep prevented"

  - platform: template
    name: "Allow Sleep"
    on_press:
      - h2_deep_sleep.allow:
          id: deep_sleep_ctrl
      - logger.log: "Sleep allowed"
```

### 示例 5: 低功耗传感器节点

```yaml
h2_deep_sleep:
  id: deep_sleep_ctrl
  run_duration: 15s
  sleep_duration: 10min
  wakeup_pin:
    number: GPIO10
    allow_other_uses: true
  wakeup_pin_mode: INVERT_WAKEUP

sensor:
  - platform: adc
    pin: GPIO2
    name: "Battery Voltage"
    id: battery_voltage
    update_interval: never

script:
  - id: read_and_send
    then:
      - logger.log: "Reading sensors..."
      - component.update: battery_voltage
      - delay: 1s
      # 发送数据到服务器...
      - logger.log: "Data sent, waiting for sleep..."

esphome:
  on_boot:
    - script.execute: read_and_send
```

## 🔧 开发过程 Development Notes

### 设计思路演进

#### 初始方案：继承 deep_sleep 组件
```python
# ❌ 这个方案失败了
from esphome.components import deep_sleep

EnhancedDeepSleep = enhanced_deep_sleep_ns.class_(
    "EnhancedDeepSleep",
    deep_sleep.DeepSleepComponent
)
```
**问题**: 找不到 `deep_sleep_component.h` 头文件

#### 最终方案：独立组件
```python
# ✅ 成功的方案
H2DeepSleep = h2_deep_sleep_ns.class_("H2DeepSleep", cg.Component)
```

### 核心实现要点

#### 1. 智能电平反转逻辑

```cpp
// 读取当前 GPIO 电平
int current_level = gpio_get_level((gpio_num_t)pin);

// 根据当前电平设置相反的唤醒条件
if (current_level == 0) {
    wakeup_mode = ESP_EXT1_WAKEUP_ANY_HIGH;  // LOW→HIGH 唤醒
} else {
    wakeup_mode = ESP_EXT1_WAKEUP_ALL_LOW;   // HIGH→LOW 唤醒
}
```

#### 2. 防止重复进入睡眠

```cpp
void H2DeepSleep::loop() {
  // 检查标志位
  if (this->entering_deep_sleep_) {
    return;  // 已在进入睡眠，直接返回
  }

  // ... 检查运行时长 ...

  if (elapsed >= target) {
    this->begin_sleep();  // 设置 entering_deep_sleep_ = true
  }
}
```

#### 3. FreeRTOS Task 实现

```cpp
void H2DeepSleep::begin_sleep(uint32_t sleep_duration_ms) {
  // 配置唤醒源
  this->prepare_deep_sleep(duration);

  // 创建独立任务
  xTaskCreate(
    this->deep_sleep_task,
    "deep_sleep_task",
    4096,    // 栈大小
    this,    // 传递 this 指针
    5,       // 优先级
    &this->deep_sleep_task_handle_
  );
}

void H2DeepSleep::deep_sleep_task(void *param) {
  // 等待其他任务完成
  vTaskDelay(pdMS_TO_TICKS(200));

  // 进入睡眠
  esp_deep_sleep_start();
}
```

#### 4. 支持引脚共享

```python
# Python 端
CONFIG_SCHEMA = cv.Schema({
    cv.Optional(CONF_WAKEUP_PIN): pins.internal_gpio_input_pin_schema,
})

async def to_code(config):
    if CONF_WAKEUP_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_WAKEUP_PIN])
        cg.add(var.set_wakeup_pin(pin))
```

```cpp
// C++ 端
class H2DeepSleep : public Component {
  void set_wakeup_pin(InternalGPIOPin *pin) {
    this->wakeup_pin_ = pin;
  }

protected:
  InternalGPIOPin *wakeup_pin_{nullptr};
};

// 使用时
uint8_t pin_num = this->wakeup_pin_->get_pin();
bool level = this->wakeup_pin_->digital_read();
```

## ❓ 常见问题

### Q1: 设备一进入睡眠就立即唤醒？

**A**: 这是因为 GPIO 电平没有变化。使用 `wakeup_pin_mode: INVERT_WAKEUP` 可以自动检测并设置相反的唤醒条件。

### Q2: 如何与其他组件共享 GPIO？

**A**: 在 pin 配置中添加 `allow_other_uses: true`：

```yaml
wakeup_pin:
  number: GPIO10
  allow_other_uses: true
```

### Q3: 日志显示 "Components should block for at most 30 ms"？

**A**: 这是因为 `loop()` 函数执行时间过长。已通过添加 `entering_deep_sleep_` 标志位解决。

### Q4: 如何在唤醒后执行特定任务？

**A**: 使用 `on_boot` 配合唤醒原因检测：

```yaml
esphome:
  on_boot:
    - lambda: |-
        if (id(deep_sleep_ctrl).get_wakeup_cause() == 4) {
          // 定时唤醒的处理逻辑
        }
```

### Q5: 可以不设置运行时长吗？

**A**: 可以！不设置 `run_duration` 时，需要手动调用 Action 进入睡眠：

```yaml
h2_deep_sleep:
  id: deep_sleep_ctrl
  sleep_duration: 1min
  # 不设置 run_duration

button:
  - platform: template
    name: "Sleep"
    on_press:
      - h2_deep_sleep.enter: deep_sleep_ctrl
```

## 📚 API 参考

### Actions

#### `h2_deep_sleep.enter`
进入 Deep Sleep 模式

```yaml
- h2_deep_sleep.enter:
    id: deep_sleep_ctrl
    sleep_duration: 30s  # 可选，覆盖默认值
```

#### `h2_deep_sleep.prevent`
阻止进入 Deep Sleep

```yaml
- h2_deep_sleep.prevent:
    id: deep_sleep_ctrl
```

#### `h2_deep_sleep.allow`
允许进入 Deep Sleep（取消阻止）

```yaml
- h2_deep_sleep.allow:
    id: deep_sleep_ctrl
```

### Lambda 函数可用方法

```cpp
// 获取唤醒原因代码 (0-6)
uint8_t cause = id(deep_sleep_ctrl).get_wakeup_cause();

// 获取唤醒原因字符串描述
const char* cause_str = id(deep_sleep_ctrl).get_wakeup_cause_string();

// 获取唤醒引脚电平 (0 或 1)
uint8_t level = id(deep_sleep_ctrl).get_wakeup_pin_level();

// 手动进入睡眠
id(deep_sleep_ctrl).begin_sleep();
id(deep_sleep_ctrl).begin_sleep(60000);  // 60秒

// 控制睡眠
id(deep_sleep_ctrl).prevent_deep_sleep();
id(deep_sleep_ctrl).allow_deep_sleep();
```

### 唤醒原因代码

| 代码 | 常量 | 说明 |
|------|------|------|
| 0 | `ESP_SLEEP_WAKEUP_UNDEFINED` | 重置/首次启动 |
| 2 | `ESP_SLEEP_WAKEUP_EXT0` | 外部唤醒 (RTC_IO) |
| 3 | `ESP_SLEEP_WAKEUP_EXT1` | 外部唤醒 (RTC_CNTL/GPIO) |
| 4 | `ESP_SLEEP_WAKEUP_TIMER` | 定时器唤醒 |
| 5 | `ESP_SLEEP_WAKEUP_TOUCHPAD` | 触摸唤醒 |
| 6 | `ESP_SLEEP_WAKEUP_ULP` | ULP 协处理器唤醒 |

## 🔍 调试技巧

### 启用详细日志

```yaml
logger:
  level: DEBUG
  logs:
    h2_deep_sleep: VERBOSE
```

### 查看完整启动流程

```yaml
esphome:
  on_boot:
    - priority: 800  # 早期执行
      then:
        - logger.log: "=== BOOT START ==="
    - priority: -100  # 晚期执行
      then:
        - lambda: |-
            ESP_LOGI("boot", "Wakeup: %s",
                     id(deep_sleep_ctrl).get_wakeup_cause_string());
```

### 测试不同唤醒场景

```yaml
# 脚本：模拟不同场景
script:
  - id: test_timer_wakeup
    then:
      - logger.log: "Testing timer wakeup..."
      - h2_deep_sleep.enter:
          id: deep_sleep_ctrl
          sleep_duration: 10s

  - id: test_gpio_wakeup
    then:
      - logger.log: "Testing GPIO wakeup (toggle GPIO10)..."
      - h2_deep_sleep.enter:
          id: deep_sleep_ctrl
          sleep_duration: 1min
```

## 🙏 致谢

感谢 ESPHome 社区和 Anthropic Claude 在开发过程中提供的帮助。

## 📄 许可证

MIT License - 自由使用和修改

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

---

**开发者**: [wacsy]
**最后更新**: 2026-01-19
**ESPHome 版本**: 2025.12.7+
**支持芯片**: ESP32-H2
