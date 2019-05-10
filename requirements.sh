#!/usr/bin/env bash
set -e

which apt >/dev/null 2>&1
if [ $? -eq 0 ]; then
	sudo apt install -y libmodbus-dev
else
	which yum >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		sudo yum install -y libmodbus-dev
	fi
fi
