/*
 * Fledge south service modbus plugin
 *
 * Copyright (c) 2019 OSIsoft, LLC
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <modbus_south.h>
#include <reading.h>
#include <logger.h>
#include <math.h>
#include <string.h>

/**
 * Set debug mode in the underlying modbus context. Set to
 * 1 to enable debug or zero to disable it.
 *
 * Note, this debug will appear on standard output.
 */
#define DEBUG	0

using namespace std;

/**
 * Constructor for the modbus interface, in this case it is a shell
 * that is awaiting configuration.
 *
 * The actual modbus connection can only be created once we have
 * configuration data.
 */
Modbus::Modbus() : m_modbus(0), m_tcp(false), m_port(0), m_device(""),
	m_baud(0), m_bits(0), m_stopBits(0), m_parity('E'), m_errcount(0),
	m_timeout(0.5)
{
}

/**
 * Destructor for the modbus interface
 */
Modbus::~Modbus()
{
	lock_guard<mutex> guard(m_configMutex);
	removeMap();
	modbus_free(m_modbus);
}

/**
 * Populate the Modbus plugin shell with a connection to a real modbus device.
 *
 * If a connection already exists then we are called as part of reconfiguration
 * and we should tear down that previous modbus context.
 */
void Modbus::createModbus()
{
	if (m_modbus)
	{
		modbus_free(m_modbus);
	}
	if (m_tcp)
	{
		char port[40];
		snprintf(port, sizeof(port), "%d", m_port);
		if ((m_modbus = modbus_new_tcp_pi(m_address.c_str(), port)) == NULL)
		{
			Logger::getLogger()->fatal("Modbus plugin failed to create modbus context, %s", modbus_strerror(errno));
			throw runtime_error("Failed to create modbus context");
		}
		struct timeval response_timeout;
		response_timeout.tv_sec = floor(m_timeout);
		response_timeout.tv_usec = (m_timeout - floor(m_timeout)) * 1000000;
		Logger::getLogger()->debug("Set request timeout to %d seconds, %d uSeconds",
				response_timeout.tv_sec, response_timeout.tv_usec);
#ifdef __arm__
		modbus_set_response_timeout(m_modbus, response_timeout.tv_sec, response_timeout.tv_usec);
#else
		modbus_set_response_timeout(m_modbus, &response_timeout);
#endif
	}
	else
	{
		if ((m_modbus = modbus_new_rtu(m_device.c_str(), m_baud, m_parity, m_bits, m_stopBits)) == NULL)
		{
			Logger::getLogger()->fatal("Modbus plugin failed to create modbus context, %s", modbus_strerror(errno));
			throw runtime_error("Failed to create mnodbus context");
		}
	}
#if DEBUG
	modbus_set_debug(m_modbus, true);
#endif
	errno = 0;
	if (modbus_connect(m_modbus) == -1)
	{
		Logger::getLogger()->error("Failed to connect to Modbus %s server %s, %s", (m_tcp ? "TCP" : "RTU"),
				(m_tcp ? m_address.c_str() : m_device.c_str()), modbus_strerror(errno));
		m_connected = false;
	}
	else
	{
		Logger::getLogger()->info("Modbus %s connected to %s",  (m_tcp ? "TCP" : "RTU"), (m_tcp ? m_address.c_str() : m_device.c_str()));
		m_connected = true;
	}
}

/**
 * Configure the modbus plugin. This may be either called to do initial
 * configuration or as a result of a reconfiguration. Hence it must hold
 * the mutex to prevent data being ingested mid configuration and also
 * clear out any existing configuration to prevent memory leaks.
 *
 * Since the configuration may result in changing the server the plugin
 * communicates with or the type of connection, the configure routine looks
 * for changes that might require the underlying modbus context to be
 * recreated. However it avoids recreating it when i is not required to
 * do so. 
 *
 * @param config	The configuration category
 */
void Modbus::configure(ConfigCategory *config)
{
string	device, address;
bool	recreate = false;
Logger	*log = Logger::getLogger();

	lock_guard<mutex> guard(m_configMutex);
	if (config->itemExists("protocol"))
	{
		string proto = config->getValue("protocol");
		if (!proto.compare("TCP"))
		{
			if (!m_tcp)
			{
				recreate = true;
				m_tcp = true;
			}
			if (config->itemExists("address"))
			{
				address = config->getValue("address");
				if (address.compare(m_address))
				{
					m_address = address;
					recreate = true;
				}
				if (! address.empty())		// Not empty
				{
					unsigned short port = 502;
					if (config->itemExists("port"))
					{
						string value = config->getValue("port");
						int port = (unsigned short)atoi(value.c_str());
						if (m_port != port)
						{
							m_port = port;
							recreate = true;
						}
					}
				}
			}
			if (config->itemExists("timeout"))
			{
				m_timeout = strtod(config->getValue("timeout").c_str(), NULL);
			}

		}
		else if (!proto.compare("RTU"))
		{
			if (m_tcp)
			{
				recreate = true;
				m_tcp = false;
			}
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
				if (m_device.compare(device) != 0)
				{
					m_device = device;
					recreate = true;
				}
				if (m_baud != baud)
				{
					m_baud = baud;
					recreate = true;
				}
				if (m_parity != parity)
				{
					m_parity = parity;
					recreate = true;
				}
				if (m_bits != bits)
				{
					m_bits = bits;
					recreate = true;
				}
				if (m_stopBits != stopBits)
				{
					m_stopBits = stopBits;
					recreate = true;
				}
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
	
	if (recreate)
	{
		createModbus();
	}

	if (config->itemExists("slave"))
	{
		setDefaultSlave(atoi(config->getValue("slave").c_str()));
	}

	if (config->itemExists("asset"))
	{
		setAssetName(config->getValue("asset"));
	}
	else
	{
		setAssetName("modbus");
	}

	/*
	 * Remove any previous map
	 */
	removeMap();

	// Now process the Modbus regster map
	string map = config->getValue("map");
	rapidjson::Document doc;
	doc.Parse(map.c_str());
	if (!doc.HasParseError())
	{
		if (doc.HasMember("values") && doc["values"].IsArray())
		{
			int errorCount = 0;
			const rapidjson::Value& values = doc["values"];
			for (rapidjson::Value::ConstValueIterator itr = values.Begin();
						itr != values.End(); ++itr)
			{
				int rCount = 0;
				int slaveID = getDefaultSlave();
				float scale = 1.0;
				float offset = 0.0;
				string name = "";
				string assetName = "";
				if (itr->HasMember("slave"))
				{
					if (! (*itr)["slave"].IsInt())
					{
						log->error("The value of slave in the modbus map should be an integer");
						errorCount++;
					}
					else
					{
						slaveID = (*itr)["slave"].GetInt();
					}
				}
				if (itr->HasMember("name"))
				{
					if ((*itr)["name"].IsString())
					{
						name = (*itr)["name"].GetString();
					}
					else
					{
						log->error("The value of name in the modbus map should be a string");
						errorCount++;
					}
				}
				else
				{
					log->error("Each item in the modbus map must have a name property");
					errorCount++;
					continue;
				}
				if (itr->HasMember("assetName"))
				{
					if ((*itr)["assetName"].IsString())
					{
						assetName = (*itr)["assetName"].GetString();
					}
					else
					{
						log->error("The value of assetName in the %s modbus map should be a string", name.c_str());
						errorCount++;
					}
				}
				if (itr->HasMember("scale"))
				{
					if (! (*itr)["scale"].IsNumber())
					{
						log->error("The value of scale in the %s modbus map should be a floating point number", name.c_str());
						errorCount++;
					}
					else
					{
						scale = (*itr)["scale"].GetFloat();
					}
				}
				if (itr->HasMember("offset"))
				{
					if (! (*itr)["offset"].IsNumber())
					{
						log->error("The value of offset in the %s modbus map should be a floating point number", name.c_str());
						errorCount++;
					}
					else
					{
						offset = (*itr)["offset"].GetFloat();
					}
				}
				if (itr->HasMember("coil"))
				{
					rCount++;
					if (! (*itr)["coil"].IsNumber())
					{
						log->error("The value of coil in the %s modbus map should be a number", name.c_str());
						errorCount++;
					}
					else
					{
						int coil = (*itr)["coil"].GetInt();
						addToMap(slaveID, new ModbusCoil(slaveID, createRegisterMap(assetName, name, coil, scale, offset)));
					}
				}
				if (itr->HasMember("input"))
				{
					rCount++;
					if ((*itr)["input"].IsInt())
					{
						int input = (*itr)["input"].GetInt();
						addToMap(slaveID, new ModbusInputBits(slaveID, createRegisterMap(assetName, name, input, scale, offset)));
					}
					else
					{
						log->error("The input item in the %s modbus map must be either an integer", name.c_str());
						errorCount++;
					}
				}
				if (itr->HasMember("register"))
				{
					rCount++;
					if ((*itr)["register"].IsInt())
					{
						int regNo = (*itr)["register"].GetInt();
						addToMap(slaveID, new ModbusRegister(slaveID, createRegisterMap(assetName, name, regNo, scale, offset)));
					}
					else if ((*itr)["register"].IsArray())
					{
						vector<unsigned int>	words;
						for (rapidjson::Value::ConstValueIterator itr2 = (*itr)["register"].Begin();
							itr2 != (*itr)["register"].End(); ++itr2)
						{
							if (itr2->IsInt())
							{
								words.push_back(itr2->GetInt());
							}
							else
							{
								log->error("The modbus map %s register array must contain integer values", name.c_str());
								errorCount++;
							}
						}
						addToMap(slaveID, new ModbusRegister(slaveID, createRegisterMap(assetName, name, words, scale, offset)));
					}
					else
					{
						log->error("The input item in the %s modbus map must be either an integer or an array", name.c_str());
						errorCount++;
					}
				}
				if (itr->HasMember("inputRegister"))
				{
					rCount++;
					if ((*itr)["inputRegister"].IsInt())
					{
						int regNo = (*itr)["inputRegister"].GetInt();
						addToMap(slaveID, new ModbusInputRegister(slaveID, createRegisterMap(assetName, name, regNo, scale, offset)));
					}
					else if ((*itr)["inputRegister"].IsArray())
					{
						vector<unsigned int>	words;
						for (rapidjson::Value::ConstValueIterator itr2 = (*itr)["inputRegister"].Begin();
							itr2 != (*itr)["inputRegister"].End(); ++itr2)
						{
							if (itr2->IsInt())
							{
								words.push_back(itr2->GetInt());
							}
							else
							{
								log->error("The %s modbus map input register array must contain integer values", name.c_str());
								errorCount++;
							}
						}
						addToMap(slaveID, new ModbusInputRegister(slaveID, createRegisterMap(assetName, name, words, scale, offset)));
					}
					else
					{
						log->error("The input item in the %s modbus map must be either an integer or an array", name.c_str());
						errorCount++;
					}
				}
				// Now deal with flags for the item we have just added
				if (itr->HasMember("type"))
				{
					if ((*itr)["type"].IsString())
					{
						string type = (*itr)["type"].GetString();
						if (type.compare("float") == 0)
						{
							m_lastItem->setFlag(ITEM_TYPE_FLOAT);
						}
					}
					else
					{
						log->error("The type property of %s must be a string", name.c_str());
					}
				}
				if (itr->HasMember("swap"))
				{
					if ((*itr)["swap"].IsString())
					{
						string swap =  (*itr)["swap"].GetString();
						if (swap.compare("bytes") == 0)
						{
							m_lastItem->setFlag(ITEM_SWAP_BYTES);
						}
						else if (swap.compare("words") == 0)
						{
							m_lastItem->setFlag(ITEM_SWAP_WORDS);
						}
						else if (swap.compare("both") == 0)
						{
							m_lastItem->setFlag(ITEM_SWAP_BYTES|ITEM_SWAP_WORDS);
						}
						else
						{
						log->error("The swap property of %s must be one of bytes, words or both", name.c_str());
						}
					}
					else
					{
						log->error("The swap property of %s must be a string", name.c_str());
					}
				}
				if (rCount == 0)
				{
					log->error("%s in map must have one of coil, input, register or inputRegister properties", name.c_str());
					errorCount++;
				}
				else if (rCount > 1)
				{
					log->error("%s in map must only have one of coil, input, register or inputRegister properties", name.c_str());
					errorCount++;
				}
			}
			if (errorCount)
			{
				log->error("%d errors encountered in the modbus map", errorCount);
			}
		}
		if (doc.HasMember("coils") && doc["coils"].IsObject())
		{
			for (rapidjson::Value::ConstMemberIterator itr = doc["coils"].MemberBegin();
						itr != doc["coils"].MemberEnd(); ++itr)
			{
				addToMap(new ModbusCoil(m_defaultSlave, createRegisterMap(itr->name.GetString(), itr->value.GetUint())));
			}
		}
		if (doc.HasMember("inputs") && doc["inputs"].IsObject())
		{
			for (rapidjson::Value::ConstMemberIterator itr = doc["inputs"].MemberBegin();
						itr != doc["inputs"].MemberEnd(); ++itr)
			{
				addToMap(new ModbusInputBits(m_defaultSlave, createRegisterMap(itr->name.GetString(), itr->value.GetUint())));
			}
		}
		if (doc.HasMember("registers") && doc["registers"].IsObject())
		{
			for (rapidjson::Value::ConstMemberIterator itr = doc["registers"].MemberBegin();
						itr != doc["registers"].MemberEnd(); ++itr)
			{
				addToMap(new ModbusRegister(m_defaultSlave, createRegisterMap(itr->name.GetString(), itr->value.GetUint())));
			}
		}
		if (doc.HasMember("inputRegisters") && doc["inputRegisters"].IsObject())
		{
			for (rapidjson::Value::ConstMemberIterator itr = doc["inputRegisters"].MemberBegin();
						itr != doc["inputRegisters"].MemberEnd(); ++itr)
			{
				addToMap(new ModbusInputRegister(m_defaultSlave, createRegisterMap(itr->name.GetString(), itr->value.GetUint())));
			}
		}
	}
	else
	{
		log->error("Parse error in modbus map, the map must be a valid JSON object");
	}

	optimise();
}

/**
 * Set the slave ID of the modbus node we are interacting with
 *
 * @param slave		The modbus slave ID
 */
void Modbus::setSlave(int slave)
{
	modbus_set_slave(m_modbus, slave);
}

/**
 * Create a modbus register map entry for a single of register
 *
 * @param	value		The datapoint names for this value
 * @param	registerNo	The register number
 */
Modbus::RegisterMap *
Modbus::createRegisterMap(const string& value, const unsigned int registerNo)
{
	m_lastItem = new Modbus::RegisterMap(value, registerNo, 1.0, 0.0);
	return m_lastItem;
}

/**
 * Create a modbus register map entry for a single of register
 *
 * @param	value		The datapoint names for this value
 * @param	registerNo	The register number
 * @param	scale		A scale factor to apply to this value
 * @param	offset		An offset to add to this value
 */
Modbus::RegisterMap *
Modbus::createRegisterMap(const string& value, const unsigned int registerNo, double scale, double offset)
{
	m_lastItem = new Modbus::RegisterMap(value, registerNo, scale, offset);
	return m_lastItem;
}


/**
 * Create a modbus register map entry for a single of register
 *
 * @param	assetName	The asset name to assign this entry
 * @param	value		The datapoint names for this value
 * @param	registerNo	The register number
 * @param	scale		A scale factor to apply to this value
 * @param	offset		An offset to add to this value
 */
Modbus::RegisterMap *
Modbus::createRegisterMap(const string& assetName, const string& value, const unsigned int registerNo, double scale, double offset)
{
	m_lastItem = new Modbus::RegisterMap(assetName, value, registerNo, scale, offset);
	return m_lastItem;
}

/**
 * Create a modbus register map entry for a set of registers
 *
 * @param	assetName	The asset name to assign this entry
 * @param	value		The datapoint names for this value
 * @param	registers	The set of registers to combine for this value
 * @param	scale		A scale factor to apply to this value
 * @param	offset		An offset to add to this value
 */
Modbus::RegisterMap *
Modbus::createRegisterMap(const string& assetName, const string& value, const std::vector<unsigned int> registers, double scale, double offset)
{
	m_lastItem = new Modbus::RegisterMap(assetName, value, registers, scale, offset);
	return m_lastItem;
}

/**
 * Add a entity to the modbus map
 *
 * @param entity	The modbus register/coil entity and the data associated with it
 */
void
Modbus::addToMap(ModbusEntity *entity)
{
	addToMap(m_defaultSlave, entity);
}

/**
 * Add a entity to the modbus map
 *
 * @param slave		The modbus slave ID
 * @param entity	The modbus register/coil entity and the data associated with it
 */
void
Modbus::addToMap(int slave, ModbusEntity *entity)
{
ModbusCacheManager	*manager = ModbusCacheManager::getModbusCacheManager();
RegisterMap		*map = entity->getMap();

	if (map->m_isVector)
	{
		for (int i = 0; i < map->m_registers.size(); i++)
		{
			manager->registerItem(slave, entity->getSource(), map->m_registers[i]);
		}
	}
	else
	{
		manager->registerItem(slave, entity->getSource(), map->m_registerNo);
	}
	if (m_map.find(slave) != m_map.end())
	{
		m_map[slave].push_back(entity);
	}
	else
	{
		vector<Modbus::ModbusEntity *> empty;
		m_map.insert(pair<int, vector<Modbus::ModbusEntity *> >(slave, empty));
		m_map[slave].push_back(entity);
	}
}

/**
 * Clear down the modbus map and remove all of the objects related to the map
 */
void
Modbus::removeMap()
{
	for (auto it = m_map.begin(); it != m_map.end(); it++)
	{
		for (int i = 0; i < it->second.size(); i++)
		{
			delete it->second[i];
		}
		it->second.clear();
	}
}


/**
 * Take a reading from the modbus
 */
vector<Reading *>	*Modbus::takeReading()
{
vector<Reading *>	*values = new vector<Reading *>();
ModbusCacheManager	*manager = ModbusCacheManager::getModbusCacheManager();
int			reconnects = 0;

	lock_guard<mutex> guard(m_configMutex);
	if (!m_modbus)
	{
		createModbus();
	}
	if (!m_connected)
	{
		errno = 0;
		if (modbus_connect(m_modbus) == -1)
		{
			Logger::getLogger()->error("Failed to connect to Modbus device %s: %s",
				(m_tcp ? m_address.c_str() : m_device.c_str()), modbus_strerror(errno));
			return values;
		}
		m_connected = true;
	}

	manager->populateCaches(m_modbus);

	for (auto it = m_map.cbegin(); it != m_map.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			int retryCount = 0;
retry:
			if (retryCount > 10)
			{
				Logger::getLogger()->error("Excessive retries to read modbus, aborting");
				return values;
			}
			Datapoint *dp = it->second[i]->read(m_modbus);
			if (dp)
			{
				m_errcount = 0;
				addModbusValue(values, it->second[i]->getAssetName(), dp);
			}
			else if (errno == EPIPE)
			{
				Logger::getLogger()->warn("Modbus connection lost, re-establishing the connection");
				m_connected = false;
				if (modbus_connect(m_modbus) == -1)
				{
					Logger::getLogger()->error("Failed to connect to Modbus device %s: %s",
						(m_tcp ? m_address.c_str() : m_device.c_str()), modbus_strerror(errno));
					return values;
				}
				m_connected = true;
				m_errcount = 0;
				retryCount++;
				goto retry;
			}
			else if (errno == EINVAL)
			{
				modbus_close(m_modbus);
				Logger::getLogger()->warn("Modbus invalid error, closing and re-establishing the connection");
				m_connected = false;
				if (modbus_connect(m_modbus) == -1)
				{
					Logger::getLogger()->error("Failed to connect to Modbus device %s: %s",
						(m_tcp ? m_address.c_str() : m_device.c_str()), modbus_strerror(errno));
					return values;
				}
				m_connected = true;
				m_errcount = 0;
				retryCount++;
				goto retry;
			}
			else if (errno == ECONNRESET)
			{
				modbus_close(m_modbus);
				Logger::getLogger()->warn("Modbus connection reset by peer, closing and re-establishing the connection");
				m_connected = false;
				if (modbus_connect(m_modbus) == -1)
				{
					Logger::getLogger()->error("Failed to connect to Modbus device %s: %s",
						(m_tcp ? m_address.c_str() : m_device.c_str()), modbus_strerror(errno));
					return values;
				}
				m_connected = true;
				m_errcount = 0;
				retryCount++;
				goto retry;
			}
			else if (errno == EMBBADDATA)
			{
				modbus_close(m_modbus);
				Logger::getLogger()->warn("Incorrect data response from modbus slave, closing and re-establishing the connection");
				m_connected = false;
				if (modbus_connect(m_modbus) == -1)
				{
					Logger::getLogger()->error("Failed to connect to Modbus device %s: %s",
						(m_tcp ? m_address.c_str() : m_device.c_str()), modbus_strerror(errno));
					return values;
				}
				m_connected = true;
				m_errcount = 0;
				retryCount++;
				goto retry;
			}
			else
			{
				Logger::getLogger()->warn("Failed with error '%s', errorcount %d", modbus_strerror(errno), m_errcount);
				modbus_close(m_modbus);
				m_connected = false;
				if (modbus_connect(m_modbus) == -1)
				{
					Logger::getLogger()->error("Failed to connect to Modbus device %s: %s",
						(m_tcp ? m_address.c_str() : m_device.c_str()), modbus_strerror(errno));
					return values;
				}
				m_connected = true;
				m_errcount++;
			}
			if (m_errcount > ERR_THRESHOLD)
			{
				if (reconnects++ > RECONNECT_LIMIT)
				{
					Logger::getLogger()->error("Persistant failure of Modbus reads - abprting readng cycle");
					values->clear();
					delete values;
					return NULL;
				}
				Logger::getLogger()->warn("Modbus excessive failures, closing and re-establishing the connection");
				modbus_close(m_modbus);
				m_connected = false;
				if (modbus_connect(m_modbus) == -1)
				{
					Logger::getLogger()->error("Failed to connect to Modbus device %s: %s",
						(m_tcp ? m_address.c_str() : m_device.c_str()), modbus_strerror(errno));
					return values;
				}
				m_connected = true;
				m_errcount = 0;
				goto retry;
			}
		}
	}

	return values;
}

/**
 * Add a new datapoint and potentially new reading to the array of readings we
 * will return.
 *
 * @param	readings	Vector of readings to update
 * @param	assetName	Asset to use or empty if default asset
 * @param	datapoint	Datapoint to add to new or existing reading
 */
void Modbus::addModbusValue(vector<Reading *> *readings, const string& assetName, Datapoint *datapoint)
{
	string asset = assetName.empty() ? m_assetName : assetName;

	bool found = false;
	for (auto it = readings->begin(); it != readings->end(); it++)
	{
		if ((*it)->getAssetName().compare(asset) == 0)
		{
			(*it)->addDatapoint(datapoint);
			found = true;
		}
	}
	if (found == false)
	{
		readings->push_back(new Reading(asset, datapoint));
	}
}

/**
 * Automatically round a result to an appropriate number of
 * decimal places based on the scale and offset.
 *
 * The number of decimals is calcaulted by determining the range
 * of the value (0 to 2^bits - 1) * scale + offset. Then taking
 * the log base 10 of 1 / the slope of the line that wudl be created
 * if this range was graphed.
 *
 * @param	value	The value to round
 * @param	bits	The numebr of bits that represent the range
 */
double Modbus::RegisterMap::round(double value, int bits)
{
	if (m_scale == 1.0)
	{
		return value;
	}
	int fullscale = pow(2, bits) - 1;
	double min = m_offset;
	double max = (fullscale * m_scale) + m_offset;
	double slope = (max - min) / fullscale;
	double dp = log10(1 / slope);

	int divisor = pow(10, (int)(dp + 0.5));

	return (double)((long)(value * divisor + 0.5)) / divisor;
}

/**
 * Optimise the modbus interactions so we fetch a large block of registers
 * or holding registers in a single interaction rather than one at a time.
 *
 * We only apply this optimisation to maos that use the latest mapping
 * specification.
 */
void Modbus::optimise()
{
	Logger::getLogger()->info("Creating Modbus caches");
	ModbusCacheManager::getModbusCacheManager()->createCaches();
}

/**
 * Constructor for the ModbusEntity base class
 *
 * @param slave		The modbus slave
 * @param map		The Modbus mao entry for this entity
 */
Modbus::ModbusEntity::ModbusEntity(int slave, RegisterMap *map) : m_slave(slave), m_map(map)
{
}

/**
 * Read a modbus entity
 *
 * @param modbus	The modbus connection
 * @return	Datapoint * the value read as a datapoint
 */
Datapoint *
Modbus::ModbusEntity::read(modbus_t *modbus)
{
	DatapointValue *dpv = readItem(modbus);
	if (!dpv)
	{
		return NULL;
	}
	DatapointValue dpv2 = *dpv;
	delete dpv;
	Datapoint *dp = new Datapoint(m_map->m_name, dpv2);
	return dp;
}

/**
 * Read a modbus coil
 *
 * @param modbus	The modbus connection
 * @return	DatapointValue * the value read as a datapoint value
 */
DatapointValue *
Modbus::ModbusCoil::readItem(modbus_t *modbus)
{
DatapointValue		*value = NULL;
uint8_t			coilValue;
int			rc;
ModbusCacheManager	*manager = ModbusCacheManager::getModbusCacheManager();

	errno = 0;
	if (manager->isCached(m_slave, MODBUS_COIL, m_map->m_registerNo))
	{
		value = new DatapointValue((long)manager->cachedValue(m_slave, MODBUS_COIL, m_map->m_registerNo));
	}
	else if ((rc = modbus_read_bits(modbus, m_map->m_registerNo, 1, &coilValue)) == 1)
	{
		value = new DatapointValue((long)coilValue);
	}
	else if (rc == -1)
	{
		Logger::getLogger()->error("Modbus read coil %d, %s", m_map->m_registerNo, modbus_strerror(errno));
		return NULL;
	}
	return value;
}

/**
 * Read a modbus input bits
 *
 * @param modbus	The modbus connection
 * @return	DatapointValue * the value read as a datapoint value
 */
DatapointValue *
Modbus::ModbusInputBits::readItem(modbus_t *modbus)
{
DatapointValue		*value = NULL;
uint8_t			coilValue;
int			rc;
ModbusCacheManager	*manager = ModbusCacheManager::getModbusCacheManager();

	errno = 0;
	if (manager->isCached(m_slave, MODBUS_INPUT, m_map->m_registerNo))
	{
		value = new DatapointValue((long)manager->cachedValue(m_slave, MODBUS_INPUT, m_map->m_registerNo));
	}
	else if ((rc = modbus_read_input_bits(modbus, m_map->m_registerNo, 1, &coilValue)) == 1)
	{
		value = new DatapointValue((long)coilValue);
	}
	else if (rc == -1)
	{
		Logger::getLogger()->error("Modbus read input bit %d, %s", m_map->m_registerNo, modbus_strerror(errno));
		return NULL;
	}
	return value;
}


/**
 * Read a modbus register
 *
 * @param modbus	The modbus connection
 * @return	DatapointValue * the value read as a datapoint value
 */
DatapointValue *
Modbus::ModbusRegister::readItem(modbus_t *modbus)
{
DatapointValue		*value = NULL;
uint16_t		regValue;
int			rc;
ModbusCacheManager	*manager = ModbusCacheManager::getModbusCacheManager();

	errno = 0;
	if (m_map->m_isVector)
	{
		long regValue = 0;
		bool failure = false;
		for (int a = 0; a < m_map->m_registers.size(); a++)
		{
			uint16_t val;
			if (manager->isCached(m_slave, MODBUS_REGISTER, m_map->m_registers[a]))
			{
				val = manager->cachedValue(m_slave, MODBUS_REGISTER, m_map->m_registers[a]);
				regValue |= (val << (a * 16));
			}
			else if ((rc = modbus_read_registers(modbus, m_map->m_registers[a], 1, &val)) == 1)
			{
				regValue |= (val << (a * 16));
			}
			else
			{
				Logger::getLogger()->error("Modbus read register %d, %s", m_map->m_registers[a], modbus_strerror(errno));
				failure = true;
			}
		}
		if (failure)
		{
			return NULL;
		}
		if (m_map->m_flags & ITEM_SWAP_BYTES)
		{
			unsigned long odd = regValue & 0x00ff00ff;
			unsigned long even = regValue & 0xff00ff00;
			regValue = (odd << 8) | (even >> 8);
		}
		if (m_map->m_flags & ITEM_SWAP_WORDS)
		{
			unsigned long odd = regValue & 0xffff;
			unsigned long even = regValue & 0xffff0000;
			regValue = (odd << 16) | (even >> 16);
		}
		if (m_map->m_flags & ITEM_TYPE_FLOAT)
		{
			union {
				uint32_t	ival;
				float		fval;
			} data;
			data.ival = (uint32_t)regValue;
			double finalValue = m_map->m_offset + (data.fval * m_map->m_scale);
			value = new DatapointValue(finalValue);
		}
		else
		{
			double finalValue = m_map->m_offset + (regValue * m_map->m_scale);
			finalValue = m_map->round(finalValue, 16);
			value = new DatapointValue(finalValue);
		}
	}
	else if (manager->isCached(m_slave, MODBUS_REGISTER, m_map->m_registerNo))
	{
		regValue = manager->cachedValue(m_slave, MODBUS_REGISTER, m_map->m_registerNo);
		double finalValue = m_map->m_offset + (regValue * m_map->m_scale);
		finalValue = m_map->round(finalValue, 8);
		value = new DatapointValue(finalValue);
	}
	else if ((rc = modbus_read_registers(modbus, m_map->m_registerNo, 1, &regValue)) == 1)
	{
		double finalValue = m_map->m_offset + (regValue * m_map->m_scale);
		finalValue = m_map->round(finalValue, 8);
		value = new DatapointValue(finalValue);
	}
	else if (rc == -1)
	{
		Logger::getLogger()->error("Modbus read register %d, %s", m_map->m_registerNo, modbus_strerror(errno));
		return NULL;
	}
	return value;
}


/**
 * Read a modbus input register
 *
 * @param modbus	The modbus connection
 * @return	DatapointValue * the value read as a datapoint value
 */
DatapointValue *
Modbus::ModbusInputRegister::readItem(modbus_t *modbus)
{
DatapointValue		*value = NULL;
uint16_t		regValue;
int			rc;
ModbusCacheManager	*manager = ModbusCacheManager::getModbusCacheManager();

	errno = 0;
	if (m_map->m_isVector)
	{
		long regValue = 0;
		bool failure = false;
		for (int a = 0; a < m_map->m_registers.size(); a++)
		{
			uint16_t val;
			if (manager->isCached(m_slave, MODBUS_INPUT_REGISTER, m_map->m_registers[a]))
			{
				val = manager->cachedValue(m_slave, MODBUS_INPUT_REGISTER, m_map->m_registers[a]);
				regValue |= (val << (a * 16));
			}
			else if ((rc = modbus_read_input_registers(modbus, m_map->m_registers[a], 1, &val)) == 1)
			{
				regValue |= (val << (a * 16));
			}
			else
			{
				Logger::getLogger()->error("Modbus read input register %d, %s", m_map->m_registerNo, modbus_strerror(errno));
				failure = true;
			}
		}
		if (failure)
		{
			return NULL;
		}
		if (m_map->m_flags & ITEM_SWAP_BYTES)
		{
			unsigned long odd = regValue & 0x00ff00ff;
			unsigned long even = regValue & 0xff00ff00;
			regValue = (odd << 8) | (even >> 8);
		}
		if (m_map->m_flags & ITEM_SWAP_WORDS)
		{
			unsigned long odd = regValue & 0xffff;
			unsigned long even = regValue & 0xffff0000;
			regValue = (odd << 16) | (even >> 16);
		}
		if (m_map->m_flags & ITEM_TYPE_FLOAT)
		{
			union {
				uint32_t	ival;
				float		fval;
			} data;
			data.ival = (uint32_t)regValue;
			double finalValue = m_map->m_offset + (data.fval * m_map->m_scale);
			value = new DatapointValue(finalValue);
		}
		else
		{
			double finalValue = m_map->m_offset + (regValue * m_map->m_scale);
			finalValue = m_map->round(finalValue, 16);
			value = new DatapointValue(finalValue);
		}
	}
	else if (manager->isCached(m_slave, MODBUS_INPUT_REGISTER, m_map->m_registerNo))
	{
		regValue = manager->cachedValue(m_slave, MODBUS_INPUT_REGISTER, m_map->m_registerNo);
		double finalValue = m_map->m_offset + (regValue * m_map->m_scale);
		finalValue = m_map->round(finalValue, 8);
		value = new DatapointValue(finalValue);
	}
	else if ((rc = modbus_read_input_registers(modbus, m_map->m_registerNo, 1, &regValue)) == 1)
	{
		double finalValue = m_map->m_offset + (regValue * m_map->m_scale);
		finalValue = m_map->round(finalValue, 8);
		value = new DatapointValue(finalValue);
	}
	else if (rc == -1)
	{
		Logger::getLogger()->error("Modbus read input register %d, %s", m_map->m_registerNo, modbus_strerror(errno));
		return NULL;
	}
	return value;
}
