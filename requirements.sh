#!/usr/bin/env bash

which apt >/dev/null 2>&1
if [ $? -eq 0 ]; then
	sudo apt install -y libmodbus-dev
else
	which yum >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		sudo yum -y install epel-release
		# Package of libmodbus is not available for CentOS Stream 9
		# sudo yum install -y libmodbus
		# sudo yum install -y libmodbus-devel
		cd ${HOME} && wget https://libmodbus.org/releases/libmodbus-3.0.8.tar.gz && tar -xvzf libmodbus-3.0.8.tar.gz && \
		cd libmodbus-3.0.8/ && ./configure && make && sudo make install && \
		cd ${HOME} && sudo rm -rf libmodbus-3.0.8*
	fi
fi
