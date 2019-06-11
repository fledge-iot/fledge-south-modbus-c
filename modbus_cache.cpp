/*
 * FogLAMP south service plugin
 *
 * Copyright (c) 2019 OSIsoft, LLC
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <modbus_south.h>
#include <logger.h>

using namespace std;

ModbusCacheManager *ModbusCacheManager::instance = 0;

/**
 * Constructor for the Modbus Cache Manager. A singleton class that 
 * manages the cache creation, population and use of the modbus cache
 */
ModbusCacheManager::ModbusCacheManager()
{
}

/**
 * Destructor for the Modbus Cache Manager. We only destroy the cache
 * manager when the modbus map is changed.
 */
ModbusCacheManager::~ModbusCacheManager()
{
	m_slaveCaches.clear();
	ModbusCacheManager::instance = 0;
}

/**
 * Return the signleton Modbus Cache Manager instance
 */
ModbusCacheManager *ModbusCacheManager::getModbusCacheManager()
{
	if (ModbusCacheManager::instance)
		return ModbusCacheManager::instance;
	else
		ModbusCacheManager::instance = new ModbusCacheManager();
	return ModbusCacheManager::instance;
}

/**
 * Register a modbus register with the cache manager. This is called during the 
 * processing of the Modbus map and is used to register the ranges in the modbus
 * register that are used.
 *
 * @param slave		The modbus slave
 * @param source	The source of the data, coil, input bits, registers or input registers
 */
void ModbusCacheManager::registerItem(int slave, ModbusSource source, int registerNo)
{
	if (m_slaveCaches.find(slave) != m_slaveCaches.end())
	{
		m_slaveCaches[slave]->addRegister(source, registerNo);
	}
	else
	{
		m_slaveCaches.insert(pair<int, SlaveCache *>(slave, new SlaveCache(source, registerNo)));
	}
}

/**
 * Called once the new modbus map has been processed to create the actual caches themsevles.
 */
void ModbusCacheManager::createCaches()
{
	for (map<int, SlaveCache *>::iterator it = m_slaveCaches.begin(); it != m_slaveCaches.end(); it++)
	{
		it->second->createCaches(it->first);
	}
}

/**
 * Call by the lower layers of the cache structure to add a cache range to the
 * caching structure.
 *
 * @param slave		The modbus slave ID for the cache
 * @param source	The source of modbus data; coils, input bits, registers or input registers
 * @param first		The first register in the cache
 * @param last		The last register in the cache
 */
void ModbusCacheManager::addCache(int slave, ModbusSource source, int first, int last)
{
	if (m_slaveCaches.find(slave) == m_slaveCaches.end())
	{
		Logger::getLogger()->fatal("Unable to find cache for slave %d", slave);
		throw runtime_error("Missing cache for slave");
	}
	m_slaveCaches[slave]->addCache(source, first, last);
}

/**
 * Populate the values in the caches
 *
 * @param modbus	The modbus interface
 */
void ModbusCacheManager::populateCaches(modbus_t *modbus)
{
	for (map<int, SlaveCache *>::iterator it = m_slaveCaches.begin(); it != m_slaveCaches.end(); it++)
	{
		it->second->populateCaches(modbus, it->first);
	}
}

/**
 * Determine if there is a cached value for a given modbus register
 *
 * @param slave		The modbus slave ID
 * @param source	The data source. Coils, input bits, registers or input registers
 * @param registerNo	The register number
 */
bool ModbusCacheManager::isCached(int slave, ModbusSource source, int registerNo)
{
	if (m_slaveCaches.find(slave) == m_slaveCaches.end())
		return false;
	return m_slaveCaches[slave]->isCached(source, registerNo);
}

/**
 * Return a value out of the cache
 *
 * @param slave		The modbus slave
 * @param source	The modbus source; coil, input bits, register or input register
 * @param registerNo	The register no
 */
uint16_t ModbusCacheManager::cachedValue(int slave, ModbusSource source, int registerNo)
{
	if (m_slaveCaches.find(slave) == m_slaveCaches.end())
	{
		throw runtime_error("Value is not cached");
	}
	return m_slaveCaches[slave]->cachedValue(source, registerNo);
}

/**
 * Constructor for a cache related to a particular slave
 *
 * @param slave		The modbus slave
 * @param registerNo	The register number that triggered the creation of this slave.
 */
ModbusCacheManager::SlaveCache::SlaveCache(ModbusSource source, int registerNo)
{
	m_ranges.insert(pair<ModbusSource, RegisterRanges *>(source, new RegisterRanges(registerNo)));
}

/**
 * Destructor for a slave cache. destroys the ranges for this slave.
 */
ModbusCacheManager::SlaveCache::~SlaveCache()
{
	m_ranges.clear();
}

/**
 * Add a source and register to the slave cache.
 *
 * @param source	The modbus data source. Coils, input bits, registers or input registers
 * @param registerNo	The register number in the modbus map
 */
void ModbusCacheManager::SlaveCache::addRegister(ModbusSource source, int registerNo)
{
	if (m_ranges.find(source) != m_ranges.end())
	{
		m_ranges[source]->addRegister(registerNo);
	}
	else
	{
		m_ranges.insert(pair<ModbusSource, RegisterRanges *>(source, new RegisterRanges(registerNo)));
	}
}

/**
 * Trigger the creation of the caches for a particular modbus slave
 *
 * @param slave		The modbus slave
 */
void ModbusCacheManager::SlaveCache::createCaches(int slave)
{
	for (map<ModbusSource, RegisterRanges *>::iterator it = m_ranges.begin(); it != m_ranges.end(); it++)
	{
		it->second->createCaches(slave, it->first);
	}
}

void ModbusCacheManager::SlaveCache::addCache(ModbusSource source, int first, int last)
{
	map<ModbusSource, RegisterRanges *>::iterator it = m_ranges.find(source);
	if (it != m_ranges.end())
	{
		it->second->addCache(source, first, last);
	}
}

/**
 * Populate the caches for a slave
 *
 * @param modbus	The modbus interface
 * @param slave		The modbus slave ID
 */
void ModbusCacheManager::SlaveCache::populateCaches(modbus_t *modbus, int slave)
{
	for (map<ModbusSource, RegisterRanges *>::iterator it = m_ranges.begin(); it != m_ranges.end(); it++)
	{
		it->second->populateCaches(modbus, slave);
	}
}

/**
 * Determine if a regsiter is in the cache
 *
 * @param source	The source of modbus data; Coils, Input Bits, Registers and Input Registers
 * @param registerNo	The modbus register number
 */
bool ModbusCacheManager::SlaveCache::isCached(ModbusSource source, int registerNo)
{
	if (m_ranges.find(source) != m_ranges.end())
	{
		return false;
	}
	else
	{
		return m_ranges[source]->isCached(registerNo);
	}
}

/**
 * Return the cached value of a register on a sopecific slave
 *
 * @param source	The source of modbus data; Coils, Input Bits, Registers and Input Registers
 * @param registerNo	The register number to return the cached value for
 */
uint16_t ModbusCacheManager::SlaveCache::cachedValue(ModbusSource source, int registerNo)
{
	if (m_ranges.find(source) != m_ranges.end())
	{
		throw runtime_error("Cached value for source is missing");
	}
	return m_ranges[source]->cachedValue(registerNo);
}

/**
 * Create a range of registers for a cache definition
 *
 * @param registerNo	The register number of the register that triggered this creation
 */
ModbusCacheManager::SlaveCache::RegisterRanges::RegisterRanges(int registerNo)
{
	m_ranges.insert(pair<int, int>(registerNo, registerNo));
}

/**
 * Destructor for register range
 */
ModbusCacheManager::SlaveCache::RegisterRanges::~RegisterRanges()
{
	m_ranges.clear();
}

/**
 * Add a register to the range of resisters.
 *
 * @param registerNo	The regiater number to extend the cache with
 */
void ModbusCacheManager::SlaveCache::RegisterRanges::addRegister(int registerNo)
{
bool done = false;

	// First deal with extending the start of a range
	map<int,int>::iterator it = m_ranges.find(registerNo + 1);
	if (it != m_ranges.end())
	{
		int last = it->second;
		m_ranges.erase(it);
		done = true;
		Logger::getLogger()->info("Extend range at start %d, %d", registerNo, last);
		m_ranges.insert(pair<int, int>(registerNo, last));
	}
	else
	{
		// Deal with the easy case of extending the end of a range
		for (map<int,int>::iterator it = m_ranges.begin(); it != m_ranges.end(); it++)
		{
			if (it->second == registerNo - 1)
			{
				it->second = registerNo;
				Logger::getLogger()->info("Extend range at end %d, %d", it->first, it->second);
				done = true;
				break;
			}
		}
	}


	if (done)
	{
		// We have extended one range, so we need to check to see if we need to coalese two ranges
		bool combined = false;
		for (map<int,int>::iterator it1 = m_ranges.begin(); it1 != m_ranges.end() && combined == false; it1++)
		{
			for (map<int,int>::iterator it2 = m_ranges.begin(); it2 != m_ranges.end(); it2++)
			{
				if (it2->first == it1->first && it2->second == it1->second)
				{
					continue;	// it1 and it2 point to same range
				}
				if (it1->second + 1 == it2->first)
				{
					it1->second = it2->second;
		Logger::getLogger()->info("Combined range at end %d, %d", it1->first, it1->second);
					m_ranges.erase(it2);
					combined = true;
					break;
				}
			}
		}
	}
	else
	{
		// Simply add a new range
		m_ranges.insert(pair<int, int>(registerNo, registerNo));
		Logger::getLogger()->info("Add range at end %d, %d", registerNo, registerNo);
	}
}

/**
 * Trigger the creation of the caches. We create a cache for every run of contiguous 
 * registers above a certain threshold.
 *
 * @param slave		The slave ID we are dealign with
 * @param source	The source of the data (coils, input bits, registers or input registers
 */
void ModbusCacheManager::SlaveCache::RegisterRanges::createCaches(int slave, ModbusSource source)
{
	ModbusCacheManager *manager = ModbusCacheManager::getModbusCacheManager();
	for (map<int,int>::iterator it = m_ranges.begin(); it != m_ranges.end(); it++)
	{
		if (it->second - it->first >= CACHE_THRESHOLD)
		{
			Logger::getLogger()->info("Create cache for slave %d, %s, %d to %d",
					slave, sourceToString(source), it->first, it->second);
			manager->addCache(slave, source, it->first, it->second);
		}
		else
		{
			Logger::getLogger()->info("Too small to cache for slave %d, %s, %d to %d",
					slave, sourceToString(source), it->first, it->second);
		}
	}
}

/**
 * Add a cache entry for a set of registers
 *
 * @param source	The modbus source; coils, input bits, registers, input registers
 * @param first		First register in the cache
 * @param last		Last register in the cache
 */
void ModbusCacheManager::SlaveCache::RegisterRanges::addCache(ModbusSource source, int first, int last)
{
	if (m_ranges.find(first) == m_ranges.end())
	{
		Logger::getLogger()->fatal("Unable to find range to cache %d %d", first, last);
		for (map<int,int>::iterator it = m_ranges.begin(); it != m_ranges.end(); it++)
		{
			Logger::getLogger()->info("Range %d to %d", first, last);
		}
		throw runtime_error("Cache range does not exist");
	}
	Cache *cache;
	switch (source)
	{
		case MODBUS_COIL:
			cache = new CoilCache(first, last);
			break;
		case MODBUS_INPUT:
			cache = new InputBitsCache(first, last);
			break;
		case MODBUS_REGISTER:
			cache = new RegisterCache(first, last);
			break;
		case MODBUS_INPUT_REGISTER:
			cache = new InputRegisterCache(first, last);
			break;
		default:
			Logger::getLogger()->fatal("Invalid modbus source for cache");
			throw runtime_error("Invalid modbus source for cache creation");
	}
	m_caches.insert(pair<int, Cache *>(first, cache));
}

/**
 * Populate the caches for a single slave and modbus source
 *
 * @param modbus	The modbus interface
 * @param slave		The modbus slave
 */
void ModbusCacheManager::SlaveCache::RegisterRanges::populateCaches(modbus_t *modbus, int slave)
{
	for (map<int, Cache *>::iterator it = m_caches.begin(); it != m_caches.end(); it++)
	{
		it->second->populateCache(modbus, slave);
	}
}

/**
 * Check if a register is cached
 *
 * @param registerNo	The register to check
 */
bool ModbusCacheManager::SlaveCache::RegisterRanges::isCached(int registerNo)
{
	for (map<int,int>::iterator it = m_ranges.begin(); it != m_ranges.end(); it++)
	{
		if (it->first <= registerNo && it->second >= registerNo)
		{
			map<int, Cache *>::iterator entry = m_caches.find(it->first);
			if (entry != m_caches.end())
			{
				return entry->second->isValid();
			}
		}
	}
	return false;
}

/**
 * Return the cached value for a register
 *
 * @param registerNo	The register to return
 */
uint16_t ModbusCacheManager::SlaveCache::RegisterRanges::cachedValue(int registerNo)
{
	for (map<int,int>::iterator it = m_ranges.begin(); it != m_ranges.end(); it++)
	{
		if (it->first <= registerNo && it->second >= registerNo)
		{
			map<int, Cache *>::iterator entry = m_caches.find(it->first);
			if (entry != m_caches.end())
			{
				return entry->second->cachedValue(registerNo);
			}
		}
	}
	throw runtime_error("Value is not cached");
}

/**
 * Create a cache to cache coil values
 *
 * @param first		The first coil to cache
 * @param last		The last coil to cache
 */
ModbusCacheManager::SlaveCache::RegisterRanges::CoilCache::CoilCache(int first, int last) : Cache(first, last)
{
	m_data = new uint8_t[1 + last - first];
}

/**
 * Populate the coil cache
 *
 * @param modbus	The modbus interface to use
 * @param slave		The modbus slave to connect to
 */
void ModbusCacheManager::SlaveCache::RegisterRanges::CoilCache::populateCache(modbus_t *modbus, int slave)
{
int rc;

	modbus_set_slave(modbus, slave);
	m_valid = false;
        errno = 0;
	int start = m_first;
	uint8_t *ptr = m_data;
	while (start < m_last)
	{
		int count = m_last - start + 1;
		if (count > MAX_MODBUS_BLOCK)
			count = MAX_MODBUS_BLOCK;
	        if ((rc = modbus_read_bits(modbus, start, count, ptr)) == -1)
		{
                	Logger::getLogger()->error("Modbus read coil cache %d, %d, %s", start, count, modbus_strerror(errno));
			return;
		}
	        else if (rc != count)
        	{
                	Logger::getLogger()->error("Modbus read coil cache %d, %d: short read %d", start, count, rc);
			return;
        	}
		start += count;
		ptr += count;
	}
	m_valid = true;
}

/**
 * Return the cached value of a coil
 *
 * @param registerNo	The register number to return the value for
 */
uint16_t ModbusCacheManager::SlaveCache::RegisterRanges::CoilCache::cachedValue(int registerNo)
{
	return (uint16_t)(m_data[registerNo - m_first]);
}

/**
 * Create the cache for Input Bits
 *
 * @param first		The first register in the cache
 * @param last		The last register in the cache
 */
ModbusCacheManager::SlaveCache::RegisterRanges::InputBitsCache::InputBitsCache(int first, int last) : Cache(first, last)
{
	m_data = new uint8_t[1 + last - first];
}

/**
 * Populate the Input Bits cache
 *
 * @param modbus	The modbus interface
 * @param slave		The modbus slave
 */
void ModbusCacheManager::SlaveCache::RegisterRanges::InputBitsCache::populateCache(modbus_t *modbus, int slave)
{
int rc;

	modbus_set_slave(modbus, slave);
	m_valid = false;
        errno = 0;
	int start = m_first;
	uint8_t *ptr = m_data;
	while (start < m_last)
	{
		int count = m_last - start + 1;
		if (count > MAX_MODBUS_BLOCK)
			count = MAX_MODBUS_BLOCK;
	        if ((rc = modbus_read_input_bits(modbus, start, count, ptr)) == -1)
		{
                	Logger::getLogger()->error("Modbus read input bits cache %d, %d, %s", start, count, modbus_strerror(errno));
			return;
		}
	        else if (rc != count)
        	{
                	Logger::getLogger()->error("Modbus read input bits cache %d, %d: short read %d", start, count, rc);
			return;
        	}
		start += count;
		ptr += count;
	}
	m_valid = true;
}

/**
 * Return the cached value of Input Bits register
 *
 * @param registerNo	The register number to return
 */
uint16_t ModbusCacheManager::SlaveCache::RegisterRanges::InputBitsCache::cachedValue(int registerNo)
{
	return (uint16_t)(m_data[registerNo - m_first]);
}

/**
 * Create a modbus register cache
 *
 * @param first		The first register in the cache
 * @param last		The last register in the cache
 */
ModbusCacheManager::SlaveCache::RegisterRanges::RegisterCache::RegisterCache(int first, int last) : Cache(first, last)
{
	m_data = new uint16_t[1 + last - first];
}

/**
 * Populate the cache of the modbus registers
 *
 * @param modbus	The modbus interface
 * @param slave		The modbus slave
 */
void ModbusCacheManager::SlaveCache::RegisterRanges::RegisterCache::populateCache(modbus_t *modbus, int slave)
{
int rc;

	modbus_set_slave(modbus, slave);
	m_valid = false;
        errno = 0;
	int start = m_first;
	uint16_t *ptr = m_data;
	while (start < m_last)
	{
		int count = m_last - start + 1;
		if (count > MAX_MODBUS_BLOCK)
			count = MAX_MODBUS_BLOCK;
	        if ((rc = modbus_read_registers(modbus, start, count, ptr)) == -1)
		{
                	Logger::getLogger()->error("Modbus read registers cache %d, %d, %s", start, count, modbus_strerror(errno));
			return;
		}
	        else if (rc != count)
        	{
                	Logger::getLogger()->error("Modbus read registers cache %d, %d: short read %d", start, count, rc);
			return;
        	}
		start += count;
		ptr += count;
	}
	m_valid = true;
}

/**
 * Return the cached value of a register
 *
 * @param registerNo	The number of the register to return the cache content of
 */
uint16_t ModbusCacheManager::SlaveCache::RegisterRanges::RegisterCache::cachedValue(int registerNo)
{
	return m_data[registerNo - m_first];
}

/**
 * Create an Input Register cache
 *
 * @param first		The first register in the cache
 * @param last		The last register in the cache
 */
ModbusCacheManager::SlaveCache::RegisterRanges::InputRegisterCache::InputRegisterCache(int first, int last) : Cache(first, last)
{
	m_data = new uint16_t[1 + last - first];
}

/**
 * Populate an input register cache
 *
 * @param modbus	The modbus interface
 * @param slave		The modbus slave
 */
void ModbusCacheManager::SlaveCache::RegisterRanges::InputRegisterCache::populateCache(modbus_t *modbus, int slave)
{
int rc;

	modbus_set_slave(modbus, slave);
	m_valid = false;
        errno = 0;
	int start = m_first;
	uint16_t *ptr = m_data;
	while (start < m_last)
	{
		int count = m_last - start + 1;
		if (count > MAX_MODBUS_BLOCK)
			count = MAX_MODBUS_BLOCK;
	        if ((rc = modbus_read_input_registers(modbus, start, count, ptr)) == -1)
		{
                	Logger::getLogger()->error("Modbus read input registers cache %d, %d, %s", start, count, modbus_strerror(errno));
			return;
		}
	        else if (rc != count)
        	{
                	Logger::getLogger()->error("Modbus read input registers cache %d, %d: short read %d", start, count, rc);
			return;
        	}
		start += count;
		ptr += count;
	}
	m_valid = true;
}

/**
 * Return the cached value of an input register
 *
 * @param registerNo	The register number whose cached value should be returned
 */
uint16_t ModbusCacheManager::SlaveCache::RegisterRanges::InputRegisterCache::cachedValue(int registerNo)
{
	return m_data[registerNo - m_first];
}
