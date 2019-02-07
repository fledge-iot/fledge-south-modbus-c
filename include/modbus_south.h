#ifndef _MODBUS_H
#define _MODBUS_H
/*
 * FogLAMP south service plugin
 *
 * Copyright (c) 2018 OSIsoft, LLC
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <reading.h>
#include <modbus/modbus.h>
#include <string>
#include <vector>
#include <map>

class Modbus {
	public:
		Modbus(const std::string& ip, const unsigned short port);
		Modbus(const std::string& device, int baud, char parity, int bits, int stopBits);
		~Modbus();
		void		setDefaultSlave(int slave) { m_defaultSlave = slave; };
		int		getDefaultSlave() { return m_defaultSlave; };
		void		setAssetName(const std::string& assetName) { m_assetName = assetName; };
		void		addRegister(const std::string& value, const unsigned int registerNo)
				{
					m_registers.push_back(new Modbus::RegisterMap(value, registerNo, 1.0, 0.0));
				};
		void		addCoil(const std::string& value, const unsigned int registerNo)
				{
					m_coils.push_back(new Modbus::RegisterMap(value, registerNo, 1.0, 0.0));
				};
		void		addInput(const std::string& value, const unsigned int registerNo)
				{
					m_inputs.push_back(new Modbus::RegisterMap(value, registerNo, 1.0, 0.0));
				};
		void		addInputRegister(const std::string& value, const unsigned int registerNo)
				{
					m_inputRegisters.push_back(new Modbus::RegisterMap(value, registerNo, 1.0, 0.0));
				};
		void		addRegister(const int slave, const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset);
		void		addCoil(const int slave, const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset);
		void		addInput(const int slave, const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset);
		void		addInputRegister(const int slave, const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset);
		std::vector<Reading *>		*takeReading();
	private:
		Modbus(const Modbus&);
		Modbus & 		operator=(const Modbus&);
		void		setSlave(int slave);
		void		addModbusValue(std::vector<Reading *> *readings, const std::string& assetName, Datapoint *datapoint);
		class RegisterMap {
			public:
				RegisterMap(const std::string& value, const unsigned int registerNo, double scale, double offset) :
					m_name(value), m_registerNo(registerNo), m_scale(scale), m_offset(offset), m_assetName("") {};
				RegisterMap(const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset) :
					m_name(value), m_registerNo(registerNo), m_scale(scale), m_offset(offset), m_assetName(assetName) {};
				const std::string		m_assetName;
				const std::string		m_name;
				const unsigned int		m_registerNo;
				const double			m_scale;
				const double			m_offset;
				double	round(double value, int bits);
		};
		modbus_t			*m_modbus;
		std::string			m_assetName;
		std::vector<RegisterMap *>	m_coils;
		std::vector<RegisterMap *>	m_inputs;
		std::vector<RegisterMap *>	m_registers;
		std::vector<RegisterMap *>	m_inputRegisters;
		std::map<int, std::vector<RegisterMap *>>
						m_slaveCoils;
		std::map<int, std::vector<RegisterMap *>>
						m_slaveInputs;
		std::map<int, std::vector<RegisterMap *>>
						m_slaveRegisters;
		std::map<int, std::vector<RegisterMap *>>
						m_slaveInputRegisters;
		const std::string		m_address;
		const unsigned short		m_port;
		const std::string		m_device;
		const bool			m_tcp;
		bool				m_connected;
		int				m_defaultSlave;
};
#endif
