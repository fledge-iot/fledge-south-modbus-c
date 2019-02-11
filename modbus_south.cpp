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

#define DEBUG	0

using namespace std;

/**
 * Constructor for the modbus interface for a TCP connection
 */
Modbus::Modbus(const string& ip, const unsigned short port) :
	m_address(ip), m_port(port), m_device(""), m_tcp(true)
{
	if ((m_modbus = modbus_new_tcp(ip.c_str(), port)) == NULL)
	{
		Logger::getLogger()->fatal("Modbus plugin failed to create modbus context, %s", modbus_strerror(errno));
		throw runtime_error("No modbus context");
	}
#if DEBUG
	modbus_set_debug(m_modbus, true);
#endif
	errno = 0;
	if (modbus_connect(m_modbus) == -1)
	{
		Logger::getLogger()->error("Failed to connect to Modbus TCP server %s", modbus_strerror(errno));
		m_connected = false;
	}
	else
	{
		Logger::getLogger()->info("Modbus TCP connected %s:%d", m_address.c_str(), m_port);
	}
}

/**
 * Constructor for the modbus interface for a serial connection
 */
Modbus::Modbus(const string& device, int baud, char parity, int bits, int stopBits) :
	m_device(device), m_address(""), m_port(0), m_tcp(false)
{
	if ((m_modbus = modbus_new_rtu(device.c_str(), baud, parity, bits, stopBits)) == NULL)
	{
		Logger::getLogger()->fatal("Modbus plugin failed to create modbus context, %s", modbus_strerror(errno));
		throw runtime_error("No modbus context");
	}
#if DEBUG
	modbus_set_debug(m_modbus, true);
#endif
	if (modbus_connect(m_modbus) == -1)
	{
		Logger::getLogger()->error("Failed to connect to Modbus RTU device:  %s", modbus_strerror(errno));
		m_connected = false;
	}
	m_connected = true;
	Logger::getLogger()->info("Modbus RTU connected to %s",  m_device.c_str());
}
/**
 * Destructor for the modbus interface
 */
Modbus::~Modbus()
{
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

	if (!m_connected)
	{
		errno = 0;
		if (modbus_connect(m_modbus) == -1)
		{
			Logger::getLogger()->error("Failed to connect to Modbus device %s: %s",
				(m_tcp ? m_address : m_device), modbus_strerror(errno));
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
