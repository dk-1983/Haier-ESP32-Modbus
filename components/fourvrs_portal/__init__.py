import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart, esp32
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]
ns = cg.esphome_ns.namespace("fourvrs_portal")
Portal = ns.class_("Portal", cg.Component)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Portal),
    cv.Required("rs485_uart_id"): cv.use_id(uart.UARTComponent),
    cv.Required("climate_id"): cv.use_id(climate.Climate),
    cv.Required("setup_password"): cv.All(cv.string_strict, cv.Length(min=8, max=63)),
    cv.Required("ota_password"): cv.All(cv.string_strict, cv.Length(min=16, max=64)),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_rs485(await cg.get_variable(config["rs485_uart_id"])))
    cg.add(var.set_climate(await cg.get_variable(config["climate_id"])))
    cg.add(var.set_setup_password(config["setup_password"]))
    cg.add(var.set_ota_password(config["ota_password"]))
    cg.add_library("WiFi", None)
    cg.add_library("WebServer", None)
    cg.add_library("ESPmDNS", None)
    cg.add_library("ArduinoOTA", None)
    cg.add_library("Network", None)
    cg.add_library("FS", None)
    cg.add_library("Update", None)
    esp32.include_builtin_idf_component("mqtt")
    esp32.include_builtin_idf_component("esp_http_client")
    esp32.include_builtin_idf_component("esp_https_ota")

    # Custom portal owns Wi-Fi, including its DHCP-enabled setup AP.
    esp32.add_idf_sdkconfig_option('CONFIG_ESP_WIFI_SOFTAP_SUPPORT', True)
    esp32.add_idf_sdkconfig_option('CONFIG_LWIP_DHCPS', True)
