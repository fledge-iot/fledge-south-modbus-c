Configuring the Mobus-C South Plugin
====================================

The Modbus-C plugin is one of the more complex plugins to configure and
has changed over the lifetime of the plugin as new functioanlity has been
added. The main change is in the way the modbus map, which maps the modbus
registers into asset data, has been represented. This document covers
the latest, must flexible mechanism and does not include the earlier,
simple map functionality.

The modbus configuration can be broken into two distinct sections;

- The physical connection configuration
- The mapping modbus registers and coils into asset data

As is common with most south plugins in FogLAMP there is an option to
set the name of the asset into which modbus data will be ingested. This
is the ``asset`` configuration item. The default for this asset name is ``modbus``.

Connection Configuration
------------------------

The modbus-c plugin supports two methods of connection to modbus
devices, direction connection using modbus-rtu or network connection
using modbus-tcp.

There are a number of configuration items that are related to the
connection configuration;

protocol
  The protocol to use to connect to the modbus device. This may be one
  of the values RTU or TCP for connection using modbus-RTU or modbus-TCP
  respectiviely.


address
  This item sets the address of the device to connect to if the protocol
  has been set as TCP. This may be an IP address or hostname that resolves
  to the modbus device.


port
  The port to use in a modbus TCP connection. This defaults to the
  standard modbus port of 2222 but may be changed if your modbus device
  uses a non-standard port.


device
  The device item specifies the physical device to use if the modbus-RTU
  protocol has been specified. This would normally be the /dev entry for
  a serial device that implements RS-232 or RS-485. This should connect
  to the hardware interface to which your modbus device is connected.


baud
  The baud rate to use to communicate over the serial link to the modbus
  device. The default rate is set as 9600 but this should be set to the
  speed of your modbus device.


bits
  The number of data bits to use in the serial communication, the default
  is 8 data bits which is normally suitable for most modbus devices.


stopbits
  The number of stop bits to include in the serial communication, the
  default value for this is 1, but may be set to 0 or 2.


parity
  The parity to use for the serial communication, this may be none,
  odd or even. The default parity is set to none.

Modbus Map Configuration
------------------------

The modbus map configuration only consists of two configuration items,
however one of these is a complex JSON document.

slave
  The default modbus slave ID to use for each register. This provides
  a default for any item in the map that does not include an explicit
  slave ID to use.


map
  The JSON object the defines the map of the modbus coils and registers
  to the asset data that is created. It consists of a JSON document with
  a single property, ``value`` which is an array of JSON objects, each
  of which becomes an asset value in the data ingested into FogLAMP. The
  following section will go into more detail with regards to this item.

Modbus map JSON object
----------------------

The JSON object that is the value of the ``map`` configuration item is
of the form

::

   {
      "values" : [
                        {
                                "name"     : "temperature",
                                "slave"    : 1,
                                "register" : 0,
                                "scale"    : 0.1
                        },
                        {
                                "name"     : "humidity",
                                "slave"    : 1,
                                "register" : 1,
                                "offset"   : 10
                        }
                 ]
   }


The ``values`` contains a set of objects, each of which has the following
properties that may be applied to it.

name
  The name of the data value to add to the asset in FogLAMP. Each asset
  may have several data points within it, this name is the name associated
  to that data point.


slave
  The slave ID to use when communicating to the modbus device. This is
  an optional property and if ommited the default slave ID is
  used. Generally if the device you are using only has a single slave
  ID then it is best to set the default slave ID and ommit this property
  from the modbus map.


assetName
  Put this value into a asset with the name given here rather than the
  default asset name defined for the plugin. This allows data from the
  modbus device to be split into multiple assets rather than a single
  asset. If ommitted then the default asset name is used.


coil
  The modbus coil to read to populate this data point within the
  asset. Coils are single bit values within the device. The value
  associated with the coil should be the coil number to read.


input
  The modbus discrete input to read to populate this data point within
  the asset. Inputs are single bit values within the device. The value
  associated with the input should be the input number to read.


regsiter
  The modbus input register to populate this data point within the
  asset. Registers are 16 bit values within the device. The value
  associated with the register should be either the register number or
  an array of registers. If an array is given then it represents a value
  that is made up of mutiple 16 bit values to create 32, 48 or 64 bit
  values in the asset data.


inputRegister
  The modbus holding register to populate this data point within the
  asset. Input registers are 16 bit values within the device. The value
  associated with the inputRegister should be either the register number or
  an array of registers. If an array is given then it represents a value
  that is made up of mutiple 16 bit values to create 32, 48 or 64 bit
  values in the asset data.


scale
  Apply a scale factor to the data read in this modbus map entry by
  applying this floating point multiplier to the value read from the
  modbus device. This property is optional and may be ommitted if there
  are no scale requirements for the particular data item.


offset
  Apply a fixed offset to a data value by adding this value to the value
  read from the modbus device. The offset is a floating point number
  and may be positive or negative. This is an optional property and if
  omitted no offset is added to the data value.


One of ``coil``, ``input``, ``register`` or ``inputRegister`` *must*
be present in each object within the values array. An entry should never
contain more than one of these however.
