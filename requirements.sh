#!/usr/bin/env bash

which apt >/dev/null 2>&1
if [ $? -eq 0 ]; then
	sudo apt install -y libmodbus-dev
else
	which yum >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		sudo yum -y install epel-release
		# Check OS NAME and VERSION
		OS_NAME=$(grep -o '^NAME=.*' /etc/os-release | cut -f2 -d\" | sed 's/"//g')
		OS_VERSION=$(grep -o '^VERSION_ID=.*' /etc/os-release | cut -f2 -d\" | sed 's/"//g')
		# Install libmodbus thorugh yum package only if
		if [[ ( ${OS_NAME} == *"Red Hat"* || ${OS_NAME} == *"CentOS"* ) && ${OS_VERSION} -ge "9" ]]; then
			cd ${HOME} && wget https://libmodbus.org/releases/libmodbus-3.0.8.tar.gz && tar -xvzf libmodbus-3.0.8.tar.gz && \
			cd libmodbus-3.0.8/ && ./configure && make && sudo make install && \
			cd ${HOME} && sudo rm -rf libmodbus-3.0.8*
		fi
	fi
fi
