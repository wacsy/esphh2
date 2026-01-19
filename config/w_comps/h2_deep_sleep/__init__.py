from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

CODEOWNERS = ["@wacsy"]
DEPENDENCIES = ["esp32"]

h2_deep_sleep_ns = cg.esphome_ns.namespace("h2_deep_sleep")
H2DeepSleep = h2_deep_sleep_ns.class_("H2DeepSleep", cg.Component)

# Actions
EnterDeepSleepAction = h2_deep_sleep_ns.class_(
    "EnterDeepSleepAction", automation.Action
)
PreventDeepSleepAction = h2_deep_sleep_ns.class_(
    "PreventDeepSleepAction", automation.Action
)
AllowDeepSleepAction = h2_deep_sleep_ns.class_(
    "AllowDeepSleepAction", automation.Action
)

CONF_SLEEP_DURATION = "sleep_duration"
CONF_WAKEUP_PIN = "wakeup_pin"
CONF_WAKEUP_PIN_MODE = "wakeup_pin_mode"
CONF_RUN_DURATION = "run_duration"

WakeupPinMode = h2_deep_sleep_ns.enum("WakeupPinMode")
WAKEUP_PIN_MODES = {
    "IGNORE": WakeupPinMode.WAKEUP_PIN_MODE_IGNORE,
    "KEEP_AWAKE": WakeupPinMode.WAKEUP_PIN_MODE_KEEP_AWAKE,
    "INVERT_WAKEUP": WakeupPinMode.WAKEUP_PIN_MODE_INVERT_WAKEUP,
}


def validate_pin_number(value):
    """验证GPIO引脚号"""
    if CORE.is_esp32:
        valid_pins = list(range(7, 15))  # ESP32-H2 GPIO范围
        pin_number = value["number"]
        if pin_number not in valid_pins:
            raise cv.Invalid(
                f"Invalid pin number: {value}. Valid pins are: {valid_pins}"
            )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(H2DeepSleep),
        cv.Optional(CONF_RUN_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SLEEP_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_WAKEUP_PIN): cv.All(
            pins.internal_gpio_input_pin_schema,
            validate_pin_number,
        ),
        cv.Optional(CONF_WAKEUP_PIN_MODE, default="INVERT_WAKEUP"): cv.enum(
            WAKEUP_PIN_MODES, upper=True
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_RUN_DURATION in config:
        cg.add(var.set_run_duration(config[CONF_RUN_DURATION]))

    if CONF_SLEEP_DURATION in config:
        cg.add(var.set_sleep_duration(config[CONF_SLEEP_DURATION]))

    if CONF_WAKEUP_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_WAKEUP_PIN])
        cg.add(var.set_wakeup_pin(pin))
    if CONF_WAKEUP_PIN_MODE in config:
        cg.add(var.set_wakeup_pin_mode(config[CONF_WAKEUP_PIN_MODE]))


# Actions
@automation.register_action(
    "h2_deep_sleep.enter",
    EnterDeepSleepAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(H2DeepSleep),
            cv.Optional(CONF_SLEEP_DURATION): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
)
async def deep_sleep_enter_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    if CONF_SLEEP_DURATION in config:
        template_ = await cg.templatable(config[CONF_SLEEP_DURATION], args, cg.uint32)
        cg.add(var.set_sleep_duration(template_))

    return var


@automation.register_action(
    "h2_deep_sleep.prevent",
    PreventDeepSleepAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(H2DeepSleep),
        }
    ),
)
async def deep_sleep_prevent_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "h2_deep_sleep.allow",
    AllowDeepSleepAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(H2DeepSleep),
        }
    ),
)
async def deep_sleep_allow_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
