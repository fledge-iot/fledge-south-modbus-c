/*
 * FogLAMP south plugin.
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
#define CONFIG	"{\"plugin\" : { \"description\" : \"Modbus TCP and RTU C south plugin\", " \
			"\"type\" : \"string\", \"default\" : \"ModbusC\", \"readonly\": \"true\" }, " \
		"\"asset\" : { \"description\" : \"Asset name\", "\
			"\"type\" : \"string\", \"default\" : \"modbus\", " \
			"\"order\": \"1\", \"displayName\": \"Asset Name\" }, " \
		"\"protocol\" : { \"description\" : \"Protocol\", "\
			"\"type\" : \"enumeration\", \"default\" : \"RTU\", " \
			"\"options\" : [ \"RTU\", \"TCP\"], " \
			 "\"order\": \"2\", \"displayName\": \"Protocol\" }, " \
		"\"address\" : { \"description\" : \"Address of Modbus TCP server\", " \
			"\"type\" : \"string\", \"default\" : \"127.0.0.1\", " \
			 "\"order\": \"3\", \"displayName\": \"Server Address\" }, "\
		"\"port\" : { \"description\" : \"Port of Modbus TCP server\", " \
			"\"type\" : \"integer\", \"default\" : \"2222\", " \
			 "\"order\": \"4\", \"displayName\": \"Port\" }, "\
		"\"device\" : { \"description\" : \"Device for Modbus RTU\", " \
			"\"type\" : \"string\", \"default\" : \"\", " \
			 "\"order\": \"5\", \"displayName\": \"Device\" }, "\
		"\"baud\" : { \"description\" : \"Baud rate  of Modbus RTU\", " \
			"\"type\" : \"integer\", \"default\" : \"9600\", " \
			 "\"order\": \"6\", \"displayName\": \"Baud Rate\" }, "\
		"\"bits\" : { \"description\" : \"Number of data bits for Modbus RTU\", " \
			"\"type\" : \"integer\", \"default\" : \"8\", " \
			 "\"order\": \"7\", \"displayName\": \"Number Of Data Bits\" }, "\
		"\"stopbits\" : { \"description\" : \"Number of stop bits for Modbus RTU\", " \
			"\"type\" : \"integer\", \"default\" : \"1\", " \
			 "\"order\": \"8\", \"displayName\": \"Number Of Stop Bits\" }, "\
		"\"parity\" : { \"description\" : \"Parity to use\", " \
			"\"type\" : \"string\", \"default\" : \"none\", " \
			 "\"order\": \"9\", \"displayName\": \"Parity\" }, "\
		"\"slave\" : { \"description\" : \"The Modbus device default slave ID\", " \
			"\"type\" : \"integer\", \"default\" : \"1\", " \
			 "\"order\": \"10\", \"displayName\": \"Slave ID\" }, "\
		"\"map\" : { \"description\" : \"Modbus register map\", " \
		    "\"order\": \"11\", \"displayName\": \"Register Map\", " \
			"\"type\" : \"JSON\", \"default\" : \"{ " \
				"\\\"values\\\" : [ { " \
					"\\\"name\\\" : \\\"temperature\\\", " \
					"\\\"slave\\\" : 1, " \
					"\\\"register\\\" : 0, " \
					"\\\"scale\\\" : 0.1, " \
					"\\\"offset\\\" : 0.0 " \
					"}, { " \
					"\\\"name\\\" : \\\"humidity\\\", " \
					"\\\"register\\\" : 1 " \
					"} ] " \
			"}\" } }"

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
	0,    			  // Flags
	PLUGIN_TYPE_SOUTH,        // Type
	"1.0.0",                  // Interface version
	CONFIG			  // Default configuration
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
Modbus *modbus = 0;
string	device, address;

	if (config->itemExists("protocol"))
	{
		string proto = config->getValue("protocol");
		if (!proto.compare("TCP"))
		{
			if (config->itemExists("address"))
			{
				address = config->getValue("address");
				if (! address.empty())		// Not empty
				{
					unsigned short port = 502;
					if (config->itemExists("port"))
					{
						string value = config->getValue("port");
						port = (unsigned short)atoi(value.c_str());
					}
					modbus = new Modbus(address, port);
				}
			}
		}
		else if (!proto.compare("RTU"))
		{
			if (config->itemExists("device"))
			{
				device = config->getValue("device");
				int baud = 9600;
				char parity = 'N';
				int bits = 8;
				int stopBits = 1;
				if (config->itemExists("baud"))
				{
					string value = config->getValue("baud");
					baud = atoi(value.c_str());
				}
				if (config->itemExists("parity"))
				{
					string value = config->getValue("parity");
					if (value.compare("even") == 0)
					{
						parity = 'E';
					}
					else if (value.compare("odd") == 0)
					{
						parity = 'O';
					}
					else if (value.compare("none") == 0)
					{
						parity = 'N';
					}
				}
				if (config->itemExists("bits"))
				{
					string value = config->getValue("bits");
					bits = atoi(value.c_str());
				}
				if (config->itemExists("stopBits"))
				{
					string value = config->getValue("stopBits");
					stopBits = atoi(value.c_str());
				}
				modbus = new Modbus(device, baud, parity, bits, stopBits);
			}
		}
		else
		{
			Logger::getLogger()->fatal("Modbus must specify either RTU or TCP as protocol");
		}
	}
	else
	{
		Logger::getLogger()->fatal("Modbus missing protocol specification");
		throw runtime_error("Unable to determine modbus protocol");
	}
	if (config->itemExists("slave"))
	{
		modbus->setDefaultSlave(atoi(config->getValue("slave").c_str()));
	}

	if (config->itemExists("asset"))
	{
		modbus->setAssetName(config->getValue("asset"));
	}
	else
	{
		modbus->setAssetName("modbus");
	}

	// Now process the Modbus regster map
	string map = config->getValue("map");
	rapidjson::Document doc;
	doc.Parse(map.c_str());
	if (!doc.HasParseError())
	{
		if (doc.HasMember("values") && doc["values"].IsArray())
		{
			const rapidjson::Value& values = doc["values"];
			for (rapidjson::Value::ConstValueIterator itr = values.Begin();
						itr != values.End(); ++itr)
			{
				int slaveID = modbus->getDefaultSlave();
				float scale = 1.0;
				float offset = 0.0;
				string name;
				if (itr->HasMember("slave"))
				{
					slaveID = (*itr)["slave"].GetInt();
				}
				if (itr->HasMember("name"))
				{
					name = (*itr)["name"].GetString();
				}
				if (itr->HasMember("scale"))
				{
					scale = (*itr)["scale"].GetFloat();
				}
				if (itr->HasMember("offset"))
				{
					offset = (*itr)["offset"].GetFloat();
				}
				if (itr->HasMember("coil"))
				{
					int coil = (*itr)["coil"].GetInt();
					modbus->addCoil(slaveID, name, coil, scale, offset);
				}
				if (itr->HasMember("input"))
				{
					int input = (*itr)["input"].GetInt();
					modbus->addInput(slaveID, name, input, scale, offset);
				}
				if (itr->HasMember("register"))
				{
					int regNo = (*itr)["register"].GetInt();
					modbus->addRegister(slaveID, name, regNo, scale, offset);
				}
				if (itr->HasMember("inputRegister"))
				{
					int regNo = (*itr)["inputRegister"].GetInt();
					modbus->addInputRegister(slaveID, name, regNo, scale, offset);
				}
			}
		}
		if (doc.HasMember("coils") && doc["coils"].IsObject())
		{
			for (rapidjson::Value::ConstMemberIterator itr = doc["coils"].MemberBegin();
						itr != doc["coils"].MemberEnd(); ++itr)
			{
				modbus->addCoil(itr->name.GetString(), itr->value.GetUint());
			}
		}
		if (doc.HasMember("inputs") && doc["inputs"].IsObject())
		{
			for (rapidjson::Value::ConstMemberIterator itr = doc["inputs"].MemberBegin();
						itr != doc["inputs"].MemberEnd(); ++itr)
			{
				modbus->addInput(itr->name.GetString(), itr->value.GetUint());
			}
		}
		if (doc.HasMember("registers") && doc["registers"].IsObject())
		{
			for (rapidjson::Value::ConstMemberIterator itr = doc["registers"].MemberBegin();
						itr != doc["registers"].MemberEnd(); ++itr)
			{
				modbus->addRegister(itr->name.GetString(), itr->value.GetUint());
			}
		}
		if (doc.HasMember("inputRegisters") && doc["inputRegisters"].IsObject())
		{
			for (rapidjson::Value::ConstMemberIterator itr = doc["inputRegisters"].MemberBegin();
						itr != doc["inputRegisters"].MemberEnd(); ++itr)
			{
				modbus->addInputRegister(itr->name.GetString(), itr->value.GetUint());
			}
		}
	}

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
Reading plugin_poll(PLUGIN_HANDLE *handle)
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
};
