#!/bin/bash

timeout=$(zenity --entry --title="Set Unikey Timeout" --text="Enter Length Of Timeout:")
if [[ ! -z $timeout ]];then
	. ~/Projects/KevJin97/unikey/scripts/unikey_timeout_set.sh $timeout
fi
