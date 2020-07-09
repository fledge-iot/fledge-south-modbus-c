#ifndef _MODBUS_H
#define _MODBUS_H
/*
 * Fledge south service plugin
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

#define ITEM_TYPE_FLOAT			0x0001
#define ITEM_SWAP_BYTES			0x0002
#define ITEM_SWAP_WORDS			0x0004

#define CACHE_THRESHOLD			5	// The number of contiguous registers we need before we create a cache

#define MAX_MODBUS_BLOCK		100 	// Max number of registers to read in a single call
#define ERR_THRESHOLD			2	// Threshold of error count before closing connection
#define RECONNECT_LIMIT			2	// Max reconnect attempts before failing a reading cycle

typedef enum { MODBUS_COIL, MODBUS_INPUT, MODBUS_REGISTER, MODBUS_INPUT_REGISTER } ModbusSource;

/**
 * The Modbus class.
 *
 * The class encapsulate the modbus entity itself, the mapping of modbus entities to
 * assets and datapoints.
 */
class Modbus {
	public:
		Modbus();
		~Modbus();
		void				configure(ConfigCategory *config);
		std::vector<Reading *>		*takeReading();
	private:
		class		RegisterMap;
		class		ModbusEntity;

		Modbus(const Modbus&);
		Modbus & 	operator=(const Modbus&);
		void		createModbus();
		void		setDefaultSlave(int slave) { m_defaultSlave = slave; };
		int		getDefaultSlave() { return m_defaultSlave; };
		void		setAssetName(const std::string& assetName) { m_assetName = assetName; };
		void		setSlave(int slave);
		void		removeMap();
		void		addToMap(int slave, ModbusEntity *entity);
		void		addToMap(ModbusEntity *entity);
		RegisterMap	*createRegisterMap(const std::string& value,
			       			const unsigned int registerNo,
					       	double scale, double offset);
		RegisterMap	*createRegisterMap(const std::string& assetName,
			       			const std::string& value,
					       	const unsigned int registerNo,
					       	double scale, double offset);
		RegisterMap	*createRegisterMap(const std::string& assetName,
			       			const std::string& value,
					       	const std::vector<unsigned int> registers,
					       	double scale, double offset);
		RegisterMap	*createRegisterMap(const std::string& value, const unsigned int registerNo);
		void		addModbusValue(std::vector<Reading *> *readings, const std::string& assetName, Datapoint *datapoint);
		void		optimise();
		void 		addCache(ModbusSource source, int slaveID, int first, int last);

		/**
		 * A class to implement a register map entry needed to map one or more modbus
		 * registers, coils or inputs to a datapoint.
		 */
		class RegisterMap {
			public:
				RegisterMap(const std::string& value, const unsigned int registerNo, double scale, double offset) :
					m_name(value), m_registerNo(registerNo), m_scale(scale), m_offset(offset), m_assetName(""),
				       	m_isVector(false), m_flags(0) {};
				RegisterMap(const std::string& assetName, const std::string& value, const unsigned int registerNo,
					       	double scale, double offset) :
					m_name(value), m_registerNo(registerNo), m_scale(scale), m_offset(offset), m_assetName(assetName),
				       	m_isVector(false), m_flags(0) {};
				RegisterMap(const std::string& assetName, const std::string& value, const std::vector<unsigned int> registers,
					       	double scale, double offset) :
					m_name(value), m_registers(registers), m_scale(scale), m_offset(offset), m_assetName(assetName),
				       	m_isVector(true), m_registerNo(0), m_flags(0) {};
				void				setFlag(unsigned long flag) { m_flags |= flag; };
				double				round(double value, int bits);
				const std::string		m_assetName;
				const std::string		m_name;
				const unsigned int		m_registerNo;
				const double			m_scale;
				const double			m_offset;
				const bool			m_isVector;
				unsigned long			m_flags;
				const std::vector<unsigned int> m_registers;
		};

		class Cache {
			public:
				Cache(ModbusSource source, int slaveID, int start, int end);
				~Cache();
				int		cache_size() { return m_end - m_start + 1; };
				uint16_t	*cache_data() { return m_data; };
				uint16_t	operator[] (int i) { return m_data[i - m_start]; };
				bool		inCache(int i) { return i >= m_start && i <= m_end; };
			private:
				int		m_start;
				int		m_end;
				int		m_slaveID;
				ModbusSource	m_source;
				uint16_t	*m_data;
		};

		/**
		 * A virtual class that encapsulates modbus coils, inputs, registers
		 * and holding registers.
		 *
		 * This provides a mechanism to do a read generically without the need
		 * to understand the modbus source underneath the datapoint.
		 */
		class ModbusEntity {
			public:
				ModbusEntity(int slave, RegisterMap *map);
				~ModbusEntity() { delete m_map; };
				Datapoint	*read(modbus_t *modbus);
				std::string	getAssetName() { return m_map->m_assetName; };
				virtual ModbusSource	getSource() = 0;
				RegisterMap		*getMap() { return m_map; };
			protected:
				virtual DatapointValue	*readItem(modbus_t *modbus) = 0;
				RegisterMap	*m_map;
				int		m_slave;

		};

		/**
		 * Specialisation to read Modbus Coil data
		 */
		class ModbusCoil : public ModbusEntity {
			public:
				ModbusCoil(int slave, RegisterMap *map) : ModbusEntity(slave, map) {};
				virtual ~ModbusCoil() {};
				DatapointValue	*readItem(modbus_t *modbus);
				ModbusSource	getSource() { return MODBUS_COIL; };
		};

		/**
		 * Specialisation to read Modbus Input Bits data
		 */
		class ModbusInputBits : public ModbusEntity {
			public:
				ModbusInputBits(int slave, RegisterMap *map) : ModbusEntity(slave, map) {};
				virtual ~ModbusInputBits() {};
				DatapointValue	*readItem(modbus_t *modbus);
				ModbusSource	getSource() { return MODBUS_INPUT; };
		};

		/**
		 * Specialisation to read Modbus Register data
		 */
		class ModbusRegister : public ModbusEntity {
			public:
				ModbusRegister(int slave, RegisterMap *map) : ModbusEntity(slave, map) {};
				virtual ~ModbusRegister() {};
				DatapointValue	*readItem(modbus_t *modbus);
				ModbusSource	getSource() { return MODBUS_REGISTER; };
		};

		/**
		 * Specialisation to read Modbus Input Registers data
		 */
		class ModbusInputRegister : public ModbusEntity {
			public:
				ModbusInputRegister(int slave, RegisterMap *map) : ModbusEntity(slave, map) {};
				virtual ~ModbusInputRegister() {};
				DatapointValue	*readItem(modbus_t *modbus);
				ModbusSource	getSource() { return MODBUS_INPUT_REGISTER; };
		};

		modbus_t			*m_modbus;
		std::string			m_assetName;
		std::map<int, std::vector<ModbusEntity *>>
						m_map;

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
		RegisterMap			*m_lastItem;
		int				m_errcount;


};

/**
 * The Modbus Cache Manager class.
 *
 * The Modbus Cache Manager is a complex heirarchy of classes, the manager itselfs
 * has a number of slave caches that it manages, one per modbus slave the modbus
 * plugin is workign with.
 *
 * Each modbus slave cache has a number of source caches
 * below it, one for coils, inputs bits, registers and input registers. If the slave
 * does not use a particualr source, then that cache is not present.
 *
 * Under each source there is a set od ranges, each range represents a contiguous range
 * of registers that are used in the modbus map. Only ranges that are cgreate than the
 * defined threshold length are actually cached.
 *
 * During the reading stage of the map, the cache manager is called to register items,
 * where an item is a slave, source and register num,ber tripple. The object structure
 * is built to represent those ranges.
 *
 * Once reading of the modbus map is complete the createCaches method of the manager is
 * called to create the actual caches themselves. This will recurse down to the register
 * ranges which will call the manager addCache method to add the physical cache.
 *
 * During operation of the modbus plugin the populateCaches methid is called for each poll
 * of the device. Then the isCached and cachedValue methods are called to retrieve the
 * actual data from the cache.
 */
class ModbusCacheManager {
	public:
		ModbusCacheManager();
		~ModbusCacheManager();
		static ModbusCacheManager	*getModbusCacheManager();
		void		createCaches();
		void		registerItem(int slave, ModbusSource source, int registerNo);
		void		addCache(int slave, ModbusSource source, int first, int last);
		void		populateCaches(modbus_t *modbus);
		bool		isCached(int slave, ModbusSource source, int registerNo);
		uint16_t	cachedValue(int slave, ModbusSource source, int registerNo);
	private:
		static ModbusCacheManager *instance;
		class SlaveCache {
			public:
				SlaveCache(ModbusSource, int registerNo);
				~SlaveCache();
				void		addRegister(ModbusSource source, int registerNo);
				bool		isCached(ModbusSource source, int registerNo);
				void		createCaches(int slave);
				void		addCache(ModbusSource source, int first, int last);
				void		populateCaches(modbus_t *modbus, int slave);
				uint16_t	cachedValue(ModbusSource source, int registerNo);
			private:
				class RegisterRanges {
					public:
						RegisterRanges(int registerNo);
						~RegisterRanges();
						void		addRegister(int registerNo);
						void		createCaches(int slave, ModbusSource source);
						void		addCache(ModbusSource source, int first, int last);
						void		populateCaches(modbus_t *modbus, int slave);
						bool		isCached(int registerNo);
						uint16_t	cachedValue(int registerNo);
					private:
						class Cache {
							public:
								Cache(int first, int last) : m_first(first), m_last(last), m_valid(false) {};
								virtual void		populateCache(modbus_t *modbus, int slave) = 0;
								virtual uint16_t	cachedValue(int registerNo) = 0;
								bool			isValid() { return m_valid; };
							protected:
								int	m_first;
								int	m_last;
								bool	m_valid;
						};
						class CoilCache : public Cache {
							public:
								CoilCache(int first, int last);
								~CoilCache() { delete[] m_data; };
								void		populateCache(modbus_t *modbus, int slave);
								uint16_t	cachedValue(int registerNo);
							private:
								uint8_t		*m_data;
						};
						class InputBitsCache : public Cache {
							public:
								InputBitsCache(int first, int last);
								~InputBitsCache() { delete[] m_data; };
								void		populateCache(modbus_t *modbus, int slave);
								uint16_t	cachedValue(int registerNo);
							private:
								uint8_t		*m_data;
						};
						class RegisterCache : public Cache {
							public:
								RegisterCache(int first, int last);
								~RegisterCache() { delete[] m_data; };
								void		populateCache(modbus_t *modbus, int slave);
								uint16_t	cachedValue(int registerNo);
							private:
								uint16_t	*m_data;
						};
						class InputRegisterCache : public Cache {
							public:
								InputRegisterCache(int first, int last);
								~InputRegisterCache() { delete[] m_data; };
								void		populateCache(modbus_t *modbus, int slave);
								uint16_t	cachedValue(int registerNo);
							private:
								uint16_t	*m_data;
						};
						const char *sourceToString(ModbusSource source) {
							switch (source)
							{
								case MODBUS_COIL:
									return "coil";
									break;
								case MODBUS_INPUT:
									return "input bits";
									break;
								case MODBUS_REGISTER:
									return "register";
									break;
								case MODBUS_INPUT_REGISTER:
									return "input register";
									break;
							}
						};
						std::map<int, int>	m_ranges;
						std::map<int, Cache *>	m_caches;
				};
				std::map<ModbusSource, RegisterRanges *>	m_ranges;
		};
		std::map<int, SlaveCache *>	m_slaveCaches;
};
#endif
