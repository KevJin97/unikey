#!/bin/bash

if [[ -d ../build ]]; then
	sudo setcap "cap_setgid=eip" ./../build/unikey && echo "Permissions have been granted"
else
	echo "Binary directory not found"
fi
