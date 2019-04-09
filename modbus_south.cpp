/*
 * FogLAMP south service plugin
 *
 * Copyright (c) 2018 OSIsoft, LLC
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <modbus_south.h>
#include <reading.h>
#include <logger.h>
#include <math.h>

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
Modbus::Modbus() : m_modbus(0)
{
}

/**
 * Destructor for the modbus interface
 */
Modbus::~Modbus()
{
	lock_guard<mutex> guard(m_configMutex);
	for (vector<RegisterMap *>::const_iterator it = m_registers.cbegin();
			it != m_registers.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_coils.cbegin();
			it != m_coils.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_inputs.cbegin();
			it != m_inputs.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_inputRegisters.cbegin();
			it != m_inputRegisters.cend(); ++it)
	{
		delete *it;
	}
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
		if ((m_modbus = modbus_new_tcp(m_address.c_str(), m_port)) == NULL)
		{
			Logger::getLogger()->fatal("Modbus plugin failed to create modbus context, %s", modbus_strerror(errno));
			throw runtime_error("Failed to create modbus context");
		}
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
	for (vector<RegisterMap *>::const_iterator it = m_registers.cbegin();
			it != m_registers.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_coils.cbegin();
			it != m_coils.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_inputs.cbegin();
			it != m_inputs.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_inputRegisters.cbegin();
			it != m_inputRegisters.cend(); ++it)
	{
		delete *it;
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
			bool coilRegMapDeleted = false; // clear previous map only once
			bool slaveInputRegMapDeleted = false; // clear previous map only once
			bool slaveRegsDeleted = false; // clear previous map only once
			bool slaveInputRegistersRegMapDeleted = false; // clear previous map only once
			for (rapidjson::Value::ConstValueIterator itr = values.Begin();
						itr != values.End(); ++itr)
			{
				int slaveID = getDefaultSlave();
				float scale = 1.0;
				float offset = 0.0;
				string name = "";
				string assetName = "";
				if (itr->HasMember("slave"))
				{
					slaveID = (*itr)["slave"].GetInt();
				}
				if (itr->HasMember("name"))
				{
					name = (*itr)["name"].GetString();
				}
				if (itr->HasMember("assetName"))
				{
					assetName = (*itr)["assetName"].GetString();
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
					if (!coilRegMapDeleted)
					{
						coilRegMapDeleted = true;
						if (m_slaveCoils.find(slaveID) != m_slaveCoils.end())
						{	
							for (auto & regMap : m_slaveCoils[slaveID])
								delete regMap;
							m_slaveCoils[slaveID].clear();
						}
					}
					int coil = (*itr)["coil"].GetInt();
					addCoil(slaveID, assetName, name, coil, scale, offset);
				}
				if (itr->HasMember("input"))
				{
					if (!slaveInputRegMapDeleted)
					{
						slaveInputRegMapDeleted = true;
						if (m_slaveInputs.find(slaveID) != m_slaveInputs.end())
						{	
							for (auto & regMap : m_slaveInputs[slaveID])
								delete regMap;
							m_slaveInputs[slaveID].clear();
						}
					}
					int input = (*itr)["input"].GetInt();
					addInput(slaveID, assetName, name, input, scale, offset);
				}
				if (itr->HasMember("register"))
				{
					if (!slaveRegsDeleted)
					{
						slaveRegsDeleted = true;
						if (m_slaveRegisters.find(slaveID) != m_slaveRegisters.end())
						{	
							for (auto & regMap : m_slaveRegisters[slaveID])
								delete regMap;
							m_slaveRegisters[slaveID].clear();
						}
					}
					int regNo = (*itr)["register"].GetInt();
					addRegister(slaveID, assetName, name, regNo, scale, offset);
				}
				if (itr->HasMember("inputRegister"))
				{
					if (!slaveInputRegistersRegMapDeleted)
					{
						slaveInputRegistersRegMapDeleted = true;
						if (m_slaveInputRegisters.find(slaveID) != m_slaveInputRegisters.end())
						{	
							for (auto & regMap : m_slaveInputRegisters[slaveID])
								delete regMap;
							m_slaveInputRegisters[slaveID].clear();
						}
					}
					int regNo = (*itr)["inputRegister"].GetInt();
					addInputRegister(slaveID, assetName, name, regNo, scale, offset);
				}
			}
		}
		if (doc.HasMember("coils") && doc["coils"].IsObject())
		{
			// RegisterMap objects inside m_coils have already been freed
			m_coils.clear();
			
			for (rapidjson::Value::ConstMemberIterator itr = doc["coils"].MemberBegin();
						itr != doc["coils"].MemberEnd(); ++itr)
			{
				addCoil(itr->name.GetString(), itr->value.GetUint());
			}
		}
		if (doc.HasMember("inputs") && doc["inputs"].IsObject())
		{
			// RegisterMap objects inside m_inputs have already been freed
			m_inputs.clear();
			
			for (rapidjson::Value::ConstMemberIterator itr = doc["inputs"].MemberBegin();
						itr != doc["inputs"].MemberEnd(); ++itr)
			{
				addInput(itr->name.GetString(), itr->value.GetUint());
			}
		}
		if (doc.HasMember("registers") && doc["registers"].IsObject())
		{
			// RegisterMap objects inside m_registers have already been freed
			m_registers.clear();
			
			for (rapidjson::Value::ConstMemberIterator itr = doc["registers"].MemberBegin();
						itr != doc["registers"].MemberEnd(); ++itr)
			{
				addRegister(itr->name.GetString(), itr->value.GetUint());
			}
		}
		if (doc.HasMember("inputRegisters") && doc["inputRegisters"].IsObject())
		{
			// RegisterMap objects inside m_inputRegisters have already been freed
			m_inputRegisters.clear();
			
			for (rapidjson::Value::ConstMemberIterator itr = doc["inputRegisters"].MemberBegin();
						itr != doc["inputRegisters"].MemberEnd(); ++itr)
			{
				addInputRegister(itr->name.GetString(), itr->value.GetUint());
			}
		}
	}
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
 * Add a registers for a particular slave
 *
 * @param slave		The slave ID we are referencing
 * @param value		The datapoint name for the associated reading
 * @param registerNo	The modbus register number
 */
void Modbus::addRegister(const int slave, const string& assetName, const string& value, const unsigned int registerNo, double scale, double offset)
{
	if (m_slaveRegisters.find(slave) != m_slaveRegisters.end())
	{
		m_slaveRegisters[slave].push_back(new Modbus::RegisterMap(assetName, value, registerNo, scale, offset));
	}
	else
	{
		vector<Modbus::RegisterMap *> empty;
		m_slaveRegisters.insert(pair<int, vector<Modbus::RegisterMap *> >(slave, empty));
		m_slaveRegisters[slave].push_back(new Modbus::RegisterMap(assetName, value, registerNo, scale, offset));
	}
}

/**
 * Add a coil for a particular slave
 *
 * @param slave		The slave ID we are referencing
 * @param value		The datapoint name for the associated reading
 * @param registerNo	The modbus register number
 */
void Modbus::addCoil(const int slave, const string& assetName, const string& value, const unsigned int registerNo, double scale, double offset)
{
	if (m_slaveCoils.find(slave) != m_slaveCoils.end())
	{
		m_slaveCoils[slave].push_back(new Modbus::RegisterMap(assetName, value, registerNo, scale, offset));
	}
	else
	{
		vector<Modbus::RegisterMap *> empty;
		m_slaveCoils.insert(pair<int, vector<Modbus::RegisterMap *> >(slave, empty));
		m_slaveCoils[slave].push_back(new Modbus::RegisterMap(assetName, value, registerNo, scale, offset));
	}
}

/**
 * Add an input for a particular slave
 *
 * @param slave		The slave ID we are referencing
 * @param value		The datapoint name for the associated reading
 * @param registerNo	The modbus register number
 */
void Modbus::addInput(const int slave, const string& assetName, const string& value, const unsigned int registerNo, double scale, double offset)
{
	if (m_slaveInputs.find(slave) != m_slaveInputs.end())
	{
		m_slaveInputs[slave].push_back(new Modbus::RegisterMap(assetName, value, registerNo, scale, offset));
	}
	else
	{
		vector<Modbus::RegisterMap *> empty;
		m_slaveInputs.insert(pair<int, vector<Modbus::RegisterMap *> >(slave, empty));
		m_slaveInputs[slave].push_back(new Modbus::RegisterMap(assetName, value, registerNo, scale, offset));
	}
}

/**
 * Add an input registers for a particular slave
 *
 * @param slave		The slave ID we are referencing
 * @param value		The datapoint name for the associated reading
 * @param registerNo	The modbus register number
 */
void Modbus::addInputRegister(const int slave, const string& assetName, const string& value, const unsigned int registerNo, double scale, double offset)
{
	if (m_slaveInputRegisters.find(slave) != m_slaveInputRegisters.end())
	{
		m_slaveInputRegisters[slave].push_back(new Modbus::RegisterMap(assetName, value, registerNo, scale, offset));
	}
	else
	{
		vector<Modbus::RegisterMap *> empty;
		m_slaveInputRegisters.insert(pair<int, vector<Modbus::RegisterMap *> >(slave, empty));
		m_slaveInputRegisters[slave].push_back(new Modbus::RegisterMap(assetName, value, registerNo, scale, offset));
	}
}


/**
 * Take a reading from the modbus
 */
vector<Reading *>	*Modbus::takeReading()
{
vector<Reading *>	*values = new vector<Reading *>();

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

	/*
	 * First do the readings from the default slave. This is really here to support backward compatibility.
	 */
	setSlave(m_defaultSlave);
	for (int i = 0; i < m_coils.size(); i++)
	{
		uint8_t	coilValue;
		if (modbus_read_bits(m_modbus, m_coils[i]->m_registerNo, 1, &coilValue) == 1)
		{

			DatapointValue value((long)coilValue);
			addModbusValue(values, "", new Datapoint(m_coils[i]->m_name, value));
		}
		else if (errno == EPIPE)
		{
			m_connected = false;
		}
	}
	for (int i = 0; i < m_inputs.size(); i++)
	{
		uint8_t	inputValue;
		int		rc;
		if (modbus_read_input_bits(m_modbus, m_inputs[i]->m_registerNo, 1, &inputValue) == 1)
		{
			DatapointValue value((long)inputValue);
			addModbusValue(values, "", new Datapoint(m_inputs[i]->m_name, value));
		}
		else if (rc == -1)
		{
			Logger::getLogger()->error("Modbus read input bits %d, %s", m_inputs[i]->m_registerNo, modbus_strerror(errno));
		}
		else if (errno == EPIPE)
		{
			m_connected = false;
		}
	}
	for (int i = 0; i < m_registers.size(); i++)
	{
		uint16_t	regValue;
		int		rc;
		errno = 0;
		if ((rc = modbus_read_registers(m_modbus, m_registers[i]->m_registerNo, 1, &regValue)) == 1)
		{
			DatapointValue value((long)regValue);
			addModbusValue(values, "", new Datapoint(m_registers[i]->m_name, value));
		}
		else if (rc == -1)
		{
			Logger::getLogger()->error("Modbus read register %d, %s", m_registers[i]->m_registerNo, modbus_strerror(errno));
		}
		if (errno == EPIPE)
		{
			m_connected = false;
		}
	}
	for (int i = 0; i < m_inputRegisters.size(); i++)
	{
		uint16_t	regValue;
		int		rc;
		errno = 0;
		if ((rc = modbus_read_input_registers(m_modbus, m_inputRegisters[i]->m_registerNo, 1, &regValue)) == 1)
		{
			DatapointValue value((long)regValue);
			addModbusValue(values, "", new Datapoint(m_inputRegisters[i]->m_name, value));
		}
		else if (rc == -1)
		{
			Logger::getLogger()->error("Modbus read register %d, %s", m_inputRegisters[i]->m_registerNo, modbus_strerror(errno));
		}
		if (errno == EPIPE)
		{
			m_connected = false;
		}
	}
	/*
	 * Now process items defined using the newer flexible configuration mechanism
	 */
	for (auto it = m_slaveCoils.cbegin(); it != m_slaveCoils.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			uint8_t	coilValue;
			if (modbus_read_bits(m_modbus, it->second[i]->m_registerNo, 1, &coilValue) == 1)
			{
				DatapointValue value((long)coilValue);
				addModbusValue(values, it->second[i]->m_assetName, new Datapoint(it->second[i]->m_name, value));
			}
			else if (errno == EPIPE)
			{
				m_connected = false;
			}
		}
	}
	for (auto it = m_slaveInputs.cbegin(); it != m_slaveInputs.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			uint8_t	inputValue;
			if (modbus_read_input_bits(m_modbus, it->second[i]->m_registerNo, 1, &inputValue) == 1)
			{
				double finalValue = it->second[i]->m_offset + (inputValue * it->second[i]->m_scale);
				finalValue = it->second[i]->round(finalValue, 8);
				DatapointValue value(finalValue);
				addModbusValue(values, it->second[i]->m_assetName, new Datapoint(it->second[i]->m_name, value));
			}
			else if (errno == EPIPE)
			{
				m_connected = false;
			}
		}
	}
	for (auto it = m_slaveRegisters.cbegin(); it != m_slaveRegisters.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			uint16_t	registerValue;
			if (modbus_read_registers(m_modbus, it->second[i]->m_registerNo, 1, &registerValue) == 1)
			{
				double finalValue = it->second[i]->m_offset + (registerValue * it->second[i]->m_scale);
				finalValue = it->second[i]->round(finalValue, 16);
				DatapointValue value(finalValue);
				addModbusValue(values, it->second[i]->m_assetName, new Datapoint(it->second[i]->m_name, value));
			}
			else if (errno == EPIPE)
			{
				m_connected = false;
			}
		}
	}
	for (auto it = m_slaveInputRegisters.cbegin(); it != m_slaveInputRegisters.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			uint16_t	registerValue;
			if (modbus_read_input_registers(m_modbus, it->second[i]->m_registerNo, 1, &registerValue) == 1)
			{
				double finalValue = it->second[i]->m_offset + (registerValue * it->second[i]->m_scale);
				finalValue = it->second[i]->round(finalValue, 16);
				DatapointValue value(finalValue);
				addModbusValue(values, it->second[i]->m_assetName, new Datapoint(it->second[i]->m_name, value));
			}
			else if (errno == EPIPE)
			{
				m_connected = false;
			}
		}
	}
	return values;
}

/**
 * Add a new datapoint and potentialluy new reading to the array of readings we
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
