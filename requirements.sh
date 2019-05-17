#!/usr/bin/env bash

which apt >/dev/null 2>&1
if [ $? -eq 0 ]; then
	apt install -y libmodbus-dev
else
	which yum >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		yum -y install epel-release
		yum install -y libmodbus
		yum install -y libmodbus-devel
	fi
fi
