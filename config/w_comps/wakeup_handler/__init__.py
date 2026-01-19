from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID

# 定义命名空间
wakeup_handler_ns = cg.esphome_ns.namespace("wakeup_handler")
WakeupHandler = wakeup_handler_ns.class_("WakeupHandler", cg.Component)

# 定义触发器
WakeupTrigger = wakeup_handler_ns.class_("WakeupTrigger", automation.Trigger.template())

# 如果是 ESP32，定义唤醒原因触发器
WakeupCauseTrigger = wakeup_handler_ns.class_(
    "WakeupCauseTrigger", automation.Trigger.template(cg.uint8)
)

# 配置常量
CONF_ON_WAKEUP = "on_wakeup"
CONF_ON_WAKEUP_CAUSE = "on_wakeup_cause"

# 唤醒原因常量 (ESP32)
WAKEUP_CAUSE_UNDEFINED = 0
WAKEUP_CAUSE_EXT0 = 2
WAKEUP_CAUSE_EXT1 = 3
WAKEUP_CAUSE_TIMER = 4
WAKEUP_CAUSE_TOUCHPAD = 5
WAKEUP_CAUSE_ULP = 6

# 配置 Schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WakeupHandler),
        cv.Optional(CONF_ON_WAKEUP): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WakeupTrigger),
            }
        ),
        cv.Optional(CONF_ON_WAKEUP_CAUSE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WakeupCauseTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # 创建组件实例
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # 配置通用唤醒触发器
    for conf in config.get(CONF_ON_WAKEUP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    # 配置带唤醒原因的触发器
    for conf in config.get(CONF_ON_WAKEUP_CAUSE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint8, "cause")], conf)
