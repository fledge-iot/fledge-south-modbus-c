#!/usr/bin/env bash

which apt >/dev/null 2>&1
if [ $? -eq 0 ]; then
	sudo apt install -y libmodbus-dev
else
	which yum >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		sudo yum -y install epel-release
		sudo yum install -y libmodbus
		sudo yum install -y libmodbus-devel
	fi
fi
