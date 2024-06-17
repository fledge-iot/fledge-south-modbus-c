/*
 * Fledge south plugin.
 *
 * Copyright (c) 2018 OSisoft, LLC
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <modbus_south.h>
#include <plugin_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <logger.h>
#include <plugin_exception.h>
#include <config_category.h>
#include <rapidjson/document.h>
#include <version.h>

using namespace std;


/**
 * Default configuration
 *
 * Note, this plugin supports two distinct Modbus maps, the original simple map
 * that allowed a single Modbus slave per plugin and a newer more flexible map.
 * It is recommended that any new implementation use the newer, flexible map format.
 *
 * Flexible map format:
 *	map : values [ <item map>, ... ]
 *
 *	item map : {
 *		name : <datapoint name>,
 *		slave : <optional slave id>,
 *		scale : <optional scale multiplier to apply>,
 *		offset : <optional data offset to add>,
 *		<modbus register>
 *
 *	modbus register :
 *		coil : <coil number>
 *		| input : <input status number>
 *		| register : <holding register number>
 *		| inputRegister : <input register number>
 */
#define MODBUS_MAP	QUOTE({					\
		"values" : [					\
			    {  					\
				"name"      : "temperature",	\
				"slave"     : 1,		\
				"assetName" : "Booth1",		\
				"register"  : 0,		\
				"scale"     : 0.1,		\
				"offset"    : 0.0		\
			    },					\
			    { 					\
				"name"      : "humidity",	\
				"register"  : 1			\
		       	    }					\
			  ]					\
		})

#define CONTROL_MAP	QUOTE({					\
		"values" : [					\
			  ]					\
		})

static const char *def_cfg = QUOTE({
		"plugin" : {
			"description" : "Modbus TCP and RTU C south plugin",
			"type" : "string",
			"default" : "ModbusC",
			"readonly": "true"
			},
		"asset" : {
			"description" : "Default asset name",
			"type" : "string",
			"default" : "modbus", 
			"order": "1",
			"displayName": "Asset Name",
			"mandatory": "true"
			}, 
		"protocol" : {
			"description" : "Protocol",
			"type" : "enumeration",
			"default" : "RTU", 
			"options" : [ "RTU", "TCP"], 
			"order": "2",
			"displayName": "Protocol"
			}, 
		"address" : {
			"description" : "Address of Modbus TCP server", 
			"type" : "string",
			"default" : "127.0.0.1", 
			"order": "3",
			"displayName": "Server Address",
			"validity": "protocol == \"TCP\""
			},
		"port" : {
			"description" : "Port of Modbus TCP server", 
			"type" : "integer",
			"default" : "2222", 
			"order": "4",
			"displayName": "Port",
			"validity" : "protocol == \"TCP\"",
			"mandatory": "true"
			},
		"device" : {
			"description" : "Device for Modbus RTU",
			"type" : "string",
			"default" : "",
			"order": "5",
			"displayName": "Device",
			"validity" : "protocol == \"RTU\""
			},
		"baud" : {
			"description" : "Baud rate  of Modbus RTU",
			"type" : "integer",
			"default" : "9600",
			"order": "6",
			"displayName": "Baud Rate",
			"validity" : "protocol == \"RTU\""
			},
		"bits" : {
			"description" : "Number of data bits for Modbus RTU",
			"type" : "integer",
			"default" : "8",
			"order": "7",
			"displayName": "Number Of Data Bits",
			"validity" : "protocol == \"RTU\""
			},
		"stopbits" : {
			"description" : "Number of stop bits for Modbus RTU",
			"type" : "integer",
			"default" : "1",
			"order": "8",
			"displayName": "Number Of Stop Bits",
			"validity" : "protocol == \"RTU\""
			},
		"parity" : {
			"description" : "Parity to use",
			"type" : "enumeration",
			"default" : "none",
			"options" : [ "none", "odd", "even" ],
			"order": "9",
			"displayName": "Parity",
			"validity" : "protocol == \"RTU\""
			},
		"slave" : {
			"description" : "The Modbus device default slave ID",
			"type" : "integer",
			"default" : "1",
			"order": "10",
			"displayName": "Slave ID"
			},
		"readMethod" : {
			"description" : "The Modbus register reading method",
			"type" : "enumeration",
			"default" : "Efficient Block Read",
			"order": "11",
			"options": ["Efficient Block Read", "Object Read", "Single Register Read"],
			"displayName": "Read Method"
			},
		"map" : {
			"description" : "Modbus register map",
			"order": "12",
			"displayName": "Register Map", 
			"type" : "JSON",
			"default" : MODBUS_MAP
			},
		"timeout" : {
			"description" : "Modbus request timeout",
			"type" : "float",
			"default" : "0.5",
			"order": "13",
			"displayName": "Timeout",
			"validity" : "protocol == \"TCP\""
			},
		"control" : {
			"description" : "The source of the control map for the Modbus plugin. This defines which registers can be written on the Modbus device.",
			"type" : "enumeration",
			"default" : "None",
			"order": "14",
			"options" : [ "None", "Use Register Map", "Use Control Map" ],
			"displayName": "Control"
			},
		"controlmap" : {
			"description" : "Modbus control register map",
			"order": "15",
			"displayName": "Control Map", 
			"type" : "JSON",
			"default" : CONTROL_MAP,
			"validity" : "control == \"Use Control Map\""
			}
		});

/**
 * The Modbus plugin interface
 */
extern "C" {

/**
 * The plugin information structure
 */
static PLUGIN_INFORMATION info = {
	"modbus",                 // Name
	VERSION,                  // Version
	SP_CONTROL, 		  // Flags
	PLUGIN_TYPE_SOUTH,        // Type
	"2.0.0",                  // Interface version
	def_cfg			  // Default configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	return &info;
}

/**
 * Initialise the plugin, called to get the plugin handle
 */
PLUGIN_HANDLE plugin_init(ConfigCategory *config)
{
Modbus *modbus = new Modbus();

	modbus->configure(config);
	return (PLUGIN_HANDLE)modbus;
}

/**
 * Start the Async handling for the plugin
 */
void plugin_start(PLUGIN_HANDLE *handle)
{
	if (!handle)
		return;
}

/**
 * Poll for a plugin reading
 */
std::vector<Reading *> *plugin_poll(PLUGIN_HANDLE *handle)
{
Modbus *modbus = (Modbus *)handle;

	if (!handle)
		throw runtime_error("Bad plugin handle");
	return modbus->takeReading();
}

/**
 * Reconfigure the plugin
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle, string& newConfig)
{
Modbus		*modbus = (Modbus *)*handle;
ConfigCategory	config("new", newConfig);

	modbus->configure(&config);
}

/**
 * Shutdown the plugin
 */
void plugin_shutdown(PLUGIN_HANDLE *handle)
{
Modbus *modbus = (Modbus *)handle;

	if (!handle)
		throw runtime_error("Bad plugin handle");
	delete modbus;
}

/**
 * Setpoint control write operation
 */
bool plugin_write(PLUGIN_HANDLE *handle, string name, string value)
{
Modbus *modbus = (Modbus *)handle;

	if (!handle)
		return false;
	return modbus->write(name, value);
}

/**
 * Setpoint control operation. None are supported by Modbus
 */
bool plugin_operation(PLUGIN_HANDLE *handle, string operation, int parameterCount, PLUGIN_PARAMETER parameters[])
{
	return false;
}
};
