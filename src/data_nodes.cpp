/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_nodes.h"

#include <zephyr.h>
#include <soc.h>

#ifndef UNIT_TEST
#include <drivers/hwinfo.h>
#include <stm32_ll_utils.h>
#include <sys/crc.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(data_nodes, CONFIG_DATA_NODES_LOG_LEVEL);
#endif

#include <stdio.h>
#include <string.h>

#include "data_storage.h"
#include "dcdc.h"
#include "hardware.h"
#include "setup.h"
#include "thingset.h"

// can be used to configure custom data objects in separate file instead
// (e.g. data_nodes_custom.cpp)
#ifndef CONFIG_CUSTOM_DATA_NODES_FILE

const char manufacturer[] = "Libre Solar";
const char device_type[] = DT_PROP(DT_PATH(pcb), type);
const char hardware_version[] = DT_PROP(DT_PATH(pcb), version_str);
const char firmware_version[] = FIRMWARE_VERSION_ID;
uint32_t flash_size = LL_GetFlashSize();
uint32_t flash_page_size = FLASH_PAGE_SIZE;
char device_id[9];

static char auth_password[11];

#if CONFIG_LV_TERMINAL_BATTERY
#define bat_bus lv_bus
#elif CONFIG_HV_TERMINAL_BATTERY
#define bat_bus hv_bus
#endif

#if CONFIG_HV_TERMINAL_SOLAR
#define solar_bus hv_bus
#elif CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
#define solar_bus lv_bus
#endif

bool pub_serial_enable = IS_ENABLED(CONFIG_THINGSET_SERIAL_PUB_DEFAULT);

#if CONFIG_THINGSET_CAN
bool pub_can_enable = IS_ENABLED(CONFIG_THINGSET_CAN_PUB_DEFAULT);
uint16_t can_node_addr = CONFIG_THINGSET_CAN_DEFAULT_NODE_ID;
#endif

/**
 * Data Objects
 *
 * IDs from 0x00 to 0x17 consume only 1 byte, so they are reserved for output data
 * objects communicated very often (to lower the data rate for LoRa and CAN)
 *
 * Normal priority data objects (consuming 2 or more bytes) start from IDs > 23 = 0x17
 */
static DataNode data_nodes[] = {

    // DEVICE INFORMATION /////////////////////////////////////////////////////
    // using IDs >= 0x18

    TS_NODE_PATH(ID_INFO, "info", 0, NULL),

    /*{
        "title": {
            "en": "Device ID",
            "de": "Geräte ID"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_STRING(0x19, "DeviceID", device_id, sizeof(device_id),
        ID_INFO, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "Manufacturer",
            "de": "Hersteller"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_STRING(0x1A, "Manufacturer", manufacturer, 0,
        ID_INFO, TS_ANY_R, 0),

    /*{
        "title": {
                "en": "Device Type",
                "de": "Gerätetyp"
            },
            "unit": "-",
            "min": "-",
            "max": "-"
    }*/
    TS_NODE_STRING(0x1B, "DeviceType", device_type, 0,
        ID_INFO, TS_ANY_R, 0),

    /*{
        "title": {
                "en": "Hardware Version",
                "de": "Hardware Version"
            },
            "unit": "-",
            "min": "-",
            "max": "-"
    }*/
    TS_NODE_STRING(0x1C, "HardwareVersion", hardware_version, 0,
        ID_INFO, TS_ANY_R, 0),

    /*{
        "title": {
                "en": "Firmware Version",
                "de": "Firmware Version"
            },
            "unit": "-",
            "min": "-",
            "max": "-"
    }*/
    TS_NODE_STRING(0x1D, "FirmwareVersion", firmware_version, 0,
        ID_INFO, TS_ANY_R, 0),


    /*{
        "title": {
                "en": "Firmware Commit",
                "de": "Firmware Commit"
            },
            "unit": "-",
            "min": "-",
            "max": "-"
    }*/
    TS_NODE_STRING(0x1E, "FirmwareCommit", firmware_commit, 0,
        ID_INFO, TS_ANY_R, 0),

    // CONFIGURATION //////////////////////////////////////////////////////////
    // using IDs >= 0x30 except for high priority data objects

    TS_NODE_PATH(ID_CONF, "conf", 0, &data_nodes_update_conf),

    // battery settings
    /*{
        "title": {
            "en": "Battery Nominal Capacity",
            "de": "Nominale Batteriekapazität"
        },
        "unit": "Ah",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x31, "BatNom_Ah", &bat_conf_user.nominal_capacity, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Battery Recharge Voltage",
            "de": "Batterieladespannung"
        },
        "unit": "V",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x32, "BatRecharge_V", &bat_conf_user.voltage_recharge, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Battery Minimal Voltage",
            "de": "Batterieminimalspannung"
        },
        "unit": "V",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x33, "BatAbsMin_V", &bat_conf_user.voltage_absolute_min, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Battery Maximal Voltage",
            "de": "Batteriemaximalspannung"
        },
        "unit": "A",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x34, "BatChgMax_A", &bat_conf_user.charge_current_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Battery Topping Voltage",
            "de": "Batterietoppingspannung"
        },
        "unit": "V",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x35, "Topping_V", &bat_conf_user.topping_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Topping Cutoff Curent",
            "de": "Topping Cutoffstrom"
        },
        "unit": "A",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x36, "ToppingCutoff_A", &bat_conf_user.topping_current_cutoff, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Topping Duration",
            "de": "Toppingdauer"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_INT32(0x37, "ToppingCutoff_s", &bat_conf_user.topping_duration,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Enable Trickle",
            "de": "Trickle einschalten"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x38, "TrickleEn", &bat_conf_user.trickle_enabled,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Trickle Voltage",
            "de": "Tricklespannung"
        },
        "unit": "V",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x39, "Trickle_V", &bat_conf_user.trickle_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Trickle Recharge Time",
            "de": "Trickle Ladedauer"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_INT32(0x3A, "TrickleRecharge_s", &bat_conf_user.trickle_recharge_time,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Enable Equalization",
            "de": "Ausgleich einschalten"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x3B, "EqlEn", &bat_conf_user.equalization_enabled,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Equalization Voltage",
            "de": "Ausgleichsspannung"
        },
        "unit": "V",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x3C, "Eql_V", &bat_conf_user.equalization_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Equalization Current Limit",
            "de": "Maximaler Ausgleichstrom"
        },
        "unit": "A",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x3D, "Eql_A", &bat_conf_user.equalization_current_limit, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Equalization Duration",
            "de": "Ausgleichsdauer"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_INT32(0x3E, "EqlDuration_s", &bat_conf_user.equalization_duration,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Equalization Trigger Days",
            "de": "Ausgleich Aktivierungstage"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_INT32(0x3F, "EqlInterval_d", &bat_conf_user.equalization_trigger_days,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Equalization Trigger Deep  Discharge Cycles",
            "de": "Ausgeich Tiefenentladungszyklen"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_INT32(0x40, "EqlDeepDisTrigger", &bat_conf_user.equalization_trigger_deep_cycles,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Temperature Compensation",
            "de": "Temperaturausgleich"
        },
        "unit": "mV/K",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x41, "BatTempComp_mV-K", &bat_conf_user.temperature_compensation, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Battery Internal Resistance",
            "de": "Interner Batteriewiderstand"
        },
        "unit": "Ohm",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x42, "BatInt_Ohm", &bat_conf_user.internal_resistance, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Battery Wire Resistance",
            "de": "Batterie Kabelwiderstand"
        },
        "unit": "Ohm",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x43, "BatWire_Ohm", &bat_conf_user.wire_resistance, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Maximum Charge Temperature",
            "de": "Maximale Ladetemperatur"
        },
        "unit": "C",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x44, "BatChgMax_degC", &bat_conf_user.charge_temp_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Minimum Charge Temperature",
            "de": "Minimale Ladetemperatur"
        },
        "unit": "C",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x45, "BatChgMin_degC", &bat_conf_user.charge_temp_min, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Maximum discharge Temperature",
            "de": "Maximale Entladetemperatur"
        },
        "unit": "C",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x46, "BatDisMax_degC", &bat_conf_user.discharge_temp_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Minimum Discharge Temperature",
            "de": "Minimale Entladetemperatur"
        },
        "unit": "C",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x47, "BatDisMin_degC", &bat_conf_user.discharge_temp_min, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    // load settings
#if BOARD_HAS_LOAD_OUTPUT

    /*{
        "title": {
            "en": "Enable Loading",
            "de": "Laden einschalten"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x50, "LoadEnDefault", &load.enable,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Voltage Load Disconnect",
            "de": "Lastabkopplungspannung"
        },
        "unit": "V",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x51, "LoadDisconnect_V", &bat_conf_user.voltage_load_disconnect, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "Voltage Load Reconnect",
            "de": "Lastkopplungspannung"
        },
        "unit": "V",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x52, "LoadReconnect_V", &bat_conf_user.voltage_load_reconnect, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "OC Revovery Delay",
            "de": "OC Wiederherstellungsverzögerung"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_INT32(0x53, "LoadOCRecovery_s", &load.oc_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "LVD Revovery Delay",
            "de": "LVD Wiederherstellungsverzögerung"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_INT32(0x54, "LoadUVRecovery_s", &load.lvd_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),
#endif

#if BOARD_HAS_USB_OUTPUT
    /*{
        "title": {
            "en": "Enable USB Power",
            "de": "USB Power einschalten"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x55, "UsbEnDefault", &usb_pwr.enable,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    //TS_NODE_FLOAT(0x56, "UsbDisconnect_V", &bat_conf_user.voltage_load_disconnect, 2,
    //    ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    //TS_NODE_FLOAT(0x57, "UsbReconnect_V", &bat_conf_user.voltage_load_reconnect, 2,
    //    ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    /*{
        "title": {
            "en": "USB LVD Revovery Delay",
            "de": "USB LVD Wiederherstellungsverzögerung"
        },
        "unit": "s",
        "min": "0",
        "max": "-"
    }*/
    TS_NODE_INT32(0x58, "UsbUVRecovery_s", &usb_pwr.lvd_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),
#endif

#if CONFIG_THINGSET_CAN
    /*{
        "title": {
            "en": "CAN Node Address",
            "de": "CAN Node Adresse"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0x59, "CanNodeId", &can_node_addr,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),
#endif

    // INPUT DATA /////////////////////////////////////////////////////////////
    // using IDs >= 0x60

    TS_NODE_PATH(ID_INPUT, "input", 0, NULL),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Enable Load",
            "de": "Last einschalten"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x61, "LoadEn", &load.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if BOARD_HAS_USB_OUTPUT
    /*{
        "title": {
            "en": "Enable USB",
            "de": "USB einschalten"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x62, "UsbEn", &usb_pwr.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if BOARD_HAS_DCDC
    /*{
        "title": {
            "en": "Enable DCDC",
            "de": "DCDC einschalten"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x63, "DcdcEn", &dcdc.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if BOARD_HAS_PWM_PORT
    /*{
        "title": {
            "en": "Enable PWM",
            "de": "PWM einschalten"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x64, "PwmEn", &pwm_switch.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    /*{
        "title": {
            "en": "Sink Voltage Intercept",
            "de": "Senkeneinschreitspannung"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x65, "GridSink_V", &hv_bus.sink_voltage_intercept, 2,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),

    /*{
        "title": {
            "en": "Source Voltage Intercept",
            "de": "Quelleneinschreitspannung"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x66, "GridSrc_V", &hv_bus.src_voltage_intercept, 2,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

    // OUTPUT DATA ////////////////////////////////////////////////////////////
    // using IDs >= 0x70 except for high priority data objects

    TS_NODE_PATH(ID_OUTPUT, "output", 0, NULL),

    // the timestamp currently reflects the time since last reset and not an actual timestamp
    TS_NODE_UINT32(0x01, "Uptime_s", &timestamp,
        ID_OUTPUT, TS_ANY_R, PUB_SER),

    // battery related data objects
    /*{
        "title": {
            "en": "Battery Voltage",
            "de": "Batteriespannung"
        },
        "unit": "V",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x71, "Bat_V", &bat_bus.voltage, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "Battery Current",
            "de": "Batteriestrom"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x72, "Bat_A", &bat_terminal.current, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "Battery Power",
            "de": "Batterieleistung"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x73, "Bat_W", &bat_terminal.power, 2,
        ID_OUTPUT, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Battery Temperature",
            "de": "Batterietemperatur"
        },
        "unit": "C",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x74, "Bat_degC", &charger.bat_temperature, 1,
        ID_OUTPUT, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Battery Ambient Temperature",
            "de": "Batterie Umgebungstemperatur"
        },
        "unit": "C",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_BOOL(0x75, "BatTempExt", &charger.ext_temp_sensor,
        ID_OUTPUT, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "??? ",
            "de": "???"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0x76, "SOC_pct", &charger.soc, // output will be uint8_t
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "Number of Batteries",
            "de": "Anzahl Batterien"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_INT16(0x77, "NumBatteries", &lv_bus.series_multiplier,
        ID_OUTPUT, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Internal Temperature",
            "de": "Interne Temperatur"
        },
        "unit": "C",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x78, "Int_degC", &dev_stat.internal_temp, 1,
        ID_OUTPUT, TS_ANY_R, 0),

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(adc_inputs), temp_fets))
    /*{
        "title": {
            "en": "Mosfet Temperature",
            "de": "Mosfet Temperatur"
        },
        "unit": "C",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x79, "Mosfet_degC", &dcdc.temp_mosfets, 1,
        ID_OUTPUT, TS_ANY_R, 0),
#endif

    /*{
        "title": {
            "en": "Charge Target Voltage",
            "de": "???"
        },
        "unit": "V",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x7A, "ChgTarget_V", &bat_bus.sink_voltage_intercept, 2,
        ID_OUTPUT, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Charge Target Current",
            "de": "???"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x7B, "ChgTarget_A", &bat_terminal.pos_current_limit, 2,
        ID_OUTPUT, TS_ANY_R, 0),


    /*{
        "title": {
            "en": "Charge State",
            "de": "Ladezustand"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0x7C, "ChgState", &charger.state,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

#if BOARD_HAS_DCDC

    /*{
        "title": {
            "en": "DCDC State",
            "de": "???"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0x7D, "DCDCState", &dcdc.state,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR

    /*{
        "title": {
            "en": "Solar Voltage",
            "de": "???"
        },
        "unit": "V",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x80, "Solar_V", &solar_bus.voltage, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#elif CONFIG_PWM_TERMINAL_SOLAR
    TS_NODE_FLOAT(0x80, "Solar_V", &pwm_switch.ext_voltage, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    /*{
        "title": {
            "en": "Solar Terminal Current",
            "de": "???"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x81, "Solar_A", &solar_terminal.current, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    /*{
        "title": {
            "en": "Solar Terminal Power",
            "de": "???"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x82, "Solar_W", &solar_terminal.power, 2,
        ID_OUTPUT, TS_ANY_R, 0),
#endif

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Load Current",
            "de": "Laststrom"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x89, "Load_A", &load.current, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "Load Power",
            "de": "Lastleistung"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x8A, "Load_W", &load.power, 2,
        ID_OUTPUT, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Load Info",
            "de": "Lastinfromation"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_INT32(0x8B, "LoadInfo", &load.info,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if BOARD_HAS_USB_OUTPUT
    /*{
        "title": {
            "en": "USB Info",
            "de": "USB Info"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_INT32(0x8C, "UsbInfo", &usb_pwr.info,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    /*{
        "title": {
            "en": "Grid Voltage",
            "de": "Netzspannung"
        },
        "unit": "V",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x90, "Grid_V", &hv_bus.voltage, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "Grid Current",
            "de": "Netzstrom"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x91, "Grid_A", &hv_terminal.current, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "Grid Power",
            "de": "Netzleistung"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x92, "Grid_W", &hv_terminal.power, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

    /*{
        "title": {
            "en": "Error Flags",
            "de": "Fehler"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0x9F, "ErrorFlags", &dev_stat.error_flags,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    // RECORDED DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xA0

    TS_NODE_PATH(ID_REC, "rec", 0, NULL),

    // accumulated data
    /*{
        "title": {
            "en": "Total Energy input",
            "de": "Erzeugte Energie"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0x08, "SolarInTotal_Wh", &dev_stat.solar_in_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

#if BOARD_HAS_LOAD_OUTPUT
        /*{
        "title": {
            "en": "Load Out Energy",
            "de": "Lastausgang Energie"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0x09, "LoadOutTotal_Wh", &dev_stat.load_out_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    /*{
        "title": {
            "en": "Grid Import Energy",
            "de": "Bezogene Energie"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0xC1, "GridImportTotal_Wh", &dev_stat.grid_import_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "Grid Export Energy",
            "de": "Gelieferte Energie"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0xC2, "GridExportTotal_Wh", &dev_stat.grid_export_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

    /*{
        "title": {
            "en": "Battery Total Charged Energy",
            "de": "???"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0x0A, "BatChgTotal_Wh", &dev_stat.bat_chg_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),


    /*{
        "title": {
            "en": "Battery Total Discharged Energy",
            "de": "???"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0x0B, "BatDisTotal_Wh", &dev_stat.bat_dis_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "Full Charges Count",
            "de": "Vollladezyklen"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0x0C, "FullChgCount", &charger.num_full_charges,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "Deep Discharges Count",
            "de": "Entladezyklen"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0x0D, "DeepDisCount", &charger.num_deep_discharges,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_NVM),

    /*{
        "title": {
            "en": "Usable Capacity",
            "de": "Verwendbare Kapazität"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0x0E, "BatUsable_Ah", &charger.usable_capacity, 1,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_NVM),

    /*{
        "title": {
            "en": "Daily Peak Solar Power",
            "de": "Maximale Solarleistung"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0x0F, "SolarMaxDay_W", &dev_stat.solar_power_max_day,
        ID_REC, TS_ANY_R | TS_MKR_W, 0),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Daily Peak Load Power",
            "de": "Maximale Lastleistung"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0x10, "LoadMaxDay_W", &dev_stat.load_power_max_day,
        ID_REC, TS_ANY_R | TS_MKR_W, 0),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    /*{
        "title": {
            "en": "Daily Solar Negative Energy",
            "de": "???"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xA1, "SolarInDay_Wh", &solar_terminal.neg_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),
#endif

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Daily Load Positive Energy",
            "de": "???"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xA2, "LoadOutDay_Wh", &load.pos_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),
#endif
    /*{
        "title": {
            "en": "Daily Charged Energy",
            "de": "???"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xA3, "BatChgDay_Wh", &bat_terminal.pos_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "Daily  Discharged Energy",
            "de": "???"
        },
        "unit": "Wh",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xA4, "BatDisDay_Wh", &bat_terminal.neg_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "Discharged Energy (Coloumb Counter)",
            "de": "???"
        },
        "unit": "Ah",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xA5, "Dis_Ah", &charger.discharged_Ah, 0,   // coulomb counter
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),

    /*{
        "title": {
            "en": "???",
            "de": "???"
        },
        "unit": "Ah",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0xA6, "SOH_pct", &charger.soh,    // output will be uint8_t
        ID_REC, TS_ANY_R | TS_MKR_W, 0),

    /*{
        "title": {
            "en": "Days",
            "de": "Tage"
        },
        "unit": "-",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0xA7, "DayCount", &dev_stat.day_counter,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),


    // min/max recordings
    /*{
        "title": {
            "en": "Solar All-Time Peak Power",
            "de": "???"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0xB1, "SolarMaxTotal_W", &dev_stat.solar_power_max_total,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Load All-Time Peak Power",
            "de": "???"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT16(0xB2, "LoadMaxTotal_W", &dev_stat.load_power_max_total,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif
    /*{
        "title": {
            "en": "Battery All-Time Peak Voltage",
            "de": "???"
        },
        "unit": "V",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xB3, "BatMaxTotal_V", &dev_stat.battery_voltage_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "Solar All-Time Peak Voltage",
            "de": "???"
        },
        "unit": "V",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xB4, "SolarMaxTotal_V", &dev_stat.solar_voltage_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "DCDC All-Time Peak Current",
            "de": "???"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xB5, "DcdcMaxTotal_A", &dev_stat.dcdc_current_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Load All-Time Peak Current",
            "de": "???"
        },
        "unit": "A",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xB6, "LoadMaxTotal_A", &dev_stat.load_current_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

    /*{
        "title": {
            "en": "Battery All-Time Peak Temperature",
            "de": "???"
        },
        "unit": "C",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_INT16(0xB7, "BatMax_degC", &dev_stat.bat_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "Internal All-Time Peak Temperature",
            "de": "???"
        },
        "unit": "C",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_INT16(0xB8, "IntMax_degC", &dev_stat.int_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "Mosfet All-Time Peak Temperature",
            "de": "???"
        },
        "unit": "C",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_INT16(0xB9, "MosfetMax_degC", &dev_stat.mosfet_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    // CALIBRATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xD0

    TS_NODE_PATH(ID_CAL, "cal", 0, NULL),

#if BOARD_HAS_DCDC
    /*{
        "title": {
            "en": "DCDC Minimal Output Power",
            "de": "???"
        },
        "unit": "W",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xD1, "DcdcMin_W", &dcdc.output_power_min, 1,
        ID_CAL, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "???",
            "de": "???"
        },
        "unit": "V",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_FLOAT(0xD2, "SolarAbsMax_V", &dcdc.hs_voltage_max, 1,
        ID_CAL, TS_ANY_R | TS_MKR_W, PUB_NVM),

    /*{
        "title": {
            "en": "DCDC Restart Interval",
            "de": "???"
        },
        "unit": "s",
        "min": "-",
        "max": "-"
    }*/
    TS_NODE_UINT32(0xD3, "DcdcRestart_s", &dcdc.restart_interval,
        ID_CAL, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

    // FUNCTION CALLS (EXEC) //////////////////////////////////////////////////
    // using IDs >= 0xE0

    TS_NODE_PATH(ID_EXEC, "exec", 0, NULL),

    /*{
        "title": {
            "en": "Reset the Device",
            "de": "Gerät zurücksetzen"
        }
    }*/
    TS_NODE_EXEC(0xE1, "reset", &reset_device, ID_EXEC, TS_ANY_RW),
    /* 0xE2 reserved (previously used for bootloader-stm) */

    /*{
        "title": {
            "en": "Save settings to EEPROM",
            "de": "Einstellungen ins EEPROM schreiben"
        }
    }*/
    TS_NODE_EXEC(0xE3, "save-settings", &eeprom_store_data, ID_EXEC, TS_ANY_RW),

    /*{
        "title": {
            "en": "Thingset Authorization",
            "de": "Thingset Anmeldung"
        }
    }*/
    TS_NODE_EXEC(0xEE, "auth", &thingset_auth, 0, TS_ANY_RW),

    /*{
        "title": {
            "en": "Send Password",
            "de": "Sende Passwort"
        }
    }*/
    TS_NODE_STRING(0xEF, "Password", auth_password, sizeof(auth_password), 0xEE, TS_ANY_RW, 0),

    // PUBLICATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xF0

    TS_NODE_PATH(ID_PUB, "pub", 0, NULL),

    TS_NODE_PATH(0xF1, "serial", ID_PUB, NULL),
    /*{
        "title": {
            "en": "Enable/Disable serial publications",
            "de": "Serielle Publikation (de)aktivieren"
        }
    }*/
    TS_NODE_BOOL(0xF2, "Enable", &pub_serial_enable, 0xF1, TS_ANY_RW, 0),

    /*{
        "title": {
            "en": "???",
            "de": "???"
        }
    }*/
    TS_NODE_PUBSUB(0xF3, "IDs", PUB_SER, 0xF1, TS_ANY_RW, 0),

#if CONFIG_THINGSET_CAN
    TS_NODE_PATH(0xF5, "can", ID_PUB, NULL),

    /*{
        "title": {
            "en": "Enable/Disable CAN publications",
            "de": "CAN Publikation (de)aktivieren"
        }
    }*/
    TS_NODE_BOOL(0xF6, "Enable", &pub_can_enable, 0xF5, TS_ANY_RW, 0),

    /*{
        "title": {
            "en": "???",
            "de": "???"
        }
    }*/
    TS_NODE_PUBSUB(0xF7, "IDs", PUB_CAN, 0xF5, TS_ANY_RW, 0),
#endif

    // DEVICE FIRMWARE UPGRADE (DFU) //////////////////////////////////////////
    // using IDs >= 0x100

    TS_NODE_PATH(0x100, "dfu", 0, NULL),

    TS_NODE_EXEC(0x101, "bootloader-stm", &start_stm32_bootloader, 0x100, TS_ANY_RW),
    TS_NODE_UINT32(0x102, "FlashSize_KiB", &flash_size, 0x100, TS_ANY_R, 0),
    TS_NODE_UINT32(0x103, "FlashPageSize_B", &flash_page_size, 0x100, TS_ANY_R, 0),
};

ThingSet ts(data_nodes, sizeof(data_nodes)/sizeof(DataNode));

void data_nodes_update_conf()
{
    bool changed;
    if (battery_conf_check(&bat_conf_user)) {
        LOG_INF("New config valid and activated.");
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
#if BOARD_HAS_LOAD_OUTPUT
        load.set_voltage_limits(bat_conf.voltage_load_disconnect, bat_conf.voltage_load_reconnect,
            bat_conf.voltage_absolute_max);
#endif
        changed = true;
    }
    else {
        LOG_ERR("Requested config change not valid and rejected.");
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
        changed = false;
    }

    // TODO: check also for changes in Load/USB EnDefault
    changed = true; // temporary hack

    if (changed) {
        data_storage_write();
    }
}

void data_nodes_init()
{
#ifndef UNIT_TEST
    uint8_t buf[12];
    hwinfo_get_device_id(buf, sizeof(buf));

    uint64_t id64 = crc32_ieee(buf, sizeof(buf));
    id64 += ((uint64_t)CONFIG_LIBRE_SOLAR_TYPE_ID) << 32;

    uint64_to_base32(id64, device_id, sizeof(device_id), alphabet_crockford);
#endif

    data_storage_read();
    if (battery_conf_check(&bat_conf_user)) {
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
    }
    else {
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
    }
}

void thingset_auth()
{
    static const char pass_exp[] = CONFIG_THINGSET_EXPERT_PASSWORD;
    static const char pass_mkr[] = CONFIG_THINGSET_MAKER_PASSWORD;

    if (strlen(pass_exp) == strlen(auth_password) &&
        strncmp(auth_password, pass_exp, strlen(pass_exp)) == 0)
    {
        LOG_INF("Authenticated as expert user.");
        ts.set_authentication(TS_EXP_MASK | TS_USR_MASK);
    }
    else if (strlen(pass_mkr) == strlen(auth_password) &&
        strncmp(auth_password, pass_mkr, strlen(pass_mkr)) == 0)
    {
        LOG_INF("Authenticated as maker.");
        ts.set_authentication(TS_MKR_MASK | TS_USR_MASK);
    }
    else {
        LOG_INF("Reset authentication.");
        ts.set_authentication(TS_USR_MASK);
    }
}

void uint64_to_base32(uint64_t in, char *out, size_t size, const char *alphabet)
{
    // 13 is the maximum number of characters needed to encode 64-bit variable to base32
    int len = (size > 13) ? 13 : size;

    // find out actual length of output string
    for (int i = 0; i < len; i++) {
        if ((in >> (i * 5)) == 0) {
            len = i;
            break;
        }
    }

    for (int i = 0; i < len; i++) {
        out[len-i-1] = alphabet[(in >> (i * 5)) % 32];
    }
    out[len] = '\0';
}


#endif /* CUSTOM_DATA_NODES_FILE */
