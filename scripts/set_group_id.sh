#!/bin/bash

if [[ -d ../build ]]; then
	sudo setcap "cap_setgid=eip" ./../build/unikey && echo "Permissions have been granted"
	#sudo setcap 'cap_net_raw,cap_net_admin+eip' ./../build/unikey
else
	echo "Binary directory not found"
fi
