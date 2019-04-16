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
#include <config_category.h>
#include <modbus/modbus.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>

class Modbus {
	public:
		Modbus();
		~Modbus();
		void				configure(ConfigCategory *config);
		std::vector<Reading *>		*takeReading();
	private:
		Modbus(const Modbus&);
		Modbus & 	operator=(const Modbus&);
		void		createModbus();
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
		void		addRegister(const int slave, const std::string& assetName, const std::string& value, const std::vector<unsigned int> registers, double scale, double offset);
		void		addCoil(const int slave, const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset);
		void		addInput(const int slave, const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset);
		void		addInputRegister(const int slave, const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset);
		void		addInputRegister(const int slave, const std::string& assetName, const std::string& value, const std::vector<unsigned int> reisters, double scale, double offset);
		void		setSlave(int slave);
		void		addModbusValue(std::vector<Reading *> *readings, const std::string& assetName, Datapoint *datapoint);
		class RegisterMap {
			public:
				RegisterMap(const std::string& value, const unsigned int registerNo, double scale, double offset) :
					m_name(value), m_registerNo(registerNo), m_scale(scale), m_offset(offset), m_assetName(""), m_isVector(false) {};
				RegisterMap(const std::string& assetName, const std::string& value, const unsigned int registerNo, double scale, double offset) :
					m_name(value), m_registerNo(registerNo), m_scale(scale), m_offset(offset), m_assetName(assetName), m_isVector(false) {};
				RegisterMap(const std::string& assetName, const std::string& value, const std::vector<unsigned int> registers, double scale, double offset) :
					m_name(value), m_registers(registers), m_scale(scale), m_offset(offset), m_assetName(assetName), m_isVector(true), m_registerNo(0) {};
				const std::string		m_assetName;
				const std::string		m_name;
				const unsigned int		m_registerNo;
				const double			m_scale;
				const double			m_offset;
				const bool			m_isVector;
				const std::vector<unsigned int> m_registers;
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
		std::string			m_address;
		unsigned short			m_port;
		std::string			m_device;
		int				m_baud;
		int				m_bits;
		int				m_stopBits;
		char				m_parity;
		bool				m_tcp;
		bool				m_connected;
		int				m_defaultSlave;
		std::mutex			m_configMutex;
};
#endif
