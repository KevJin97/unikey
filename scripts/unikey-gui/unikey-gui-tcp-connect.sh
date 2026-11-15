#!/bin/bash
ip_addr=$(zenity --entry --title="Connect to a Unikey Server" --text="Enter IP Address:")
if [ -n "$ip_addr" ]; then
	. ~/Project/KevJin97/unikey/script/unikey_connect_to.sh $ip_addr
fi
