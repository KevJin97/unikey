#!/bin/bash

# GUI command interface for Unikey
CHOICE=$(zenity --list \
	--title="Unikey" \
	--column="Unikey Commands" \
		"Trigger" \
		"Set Timeout" \
		"Connect to IP Address" \
		"Toggle Bluetooth" \
		"Exit")

# Initiate Unikey if it's not already running
if ! busctl list | grep -q "io.unikey"; then
	# If it's not running and command isn't "Exit", start an instance of Unikey
	if [[ ! -z "$CHOICE" ]] && [[ "$CHOICE" != "Exit" ]]; then
		~/repos/unikey/build/unikey & disown
	
		until busctl list | grep -q "io.unikey"; do
			sleep 0.001
		done

		notify-send --app-name="Unikey Notifications" \
			"Unikey has been initialized" \
			"$(date "+%F | %R")"
	else
		# If it's not running and command is "Exit", don't do anything
		exit
	fi
fi

# Execute corresponding command for Unikey
case $CHOICE in
	"Trigger")
		busctl --system call io.unikey \
			/io/unikey/Device \
			io.unikey.Device.Methods \
			Trigger
		;;
	"Set Timeout")
		timeout=$(zenity --entry --title="Set Unikey Timeout" --text="Enter Length Of Timeout:")
		if [[ ! -z $timeout ]]; then
			busctl --system call io.unikey \
				/io/unikey/Device \
				io.unikey.Device.Methods \
				SetTimeout u $timeout
		fi	
		;;
	"Connect to IP Address")
		ip_addr=$(zenity --entry --title="Connect to a Unikey Server" --text="Enter IP Address:")
		if [ -n "$ip_addr" ]; then	
			busctl --system call io.unikey \
				/io/unikey/WiFi \
				io.unikey.WiFi.Methods \
				ConnectTo s "$ip_addr"
		fi
		;;
	"Toggle Bluetooth")
		busctl --system call io.unikey \
			/io/unikey/Bluetooth \
			io.unikey.Bluetooth.Methods \
			ToggleBluetooth
		;;

	"Exit")
		busctl --system call io.unikey \
			/io/unikey/Device \
			io.unikey.Device.Methods \
			Exit
		notify-send --app-name="Unikey Notifications" \
			"Unikey has been disabled" \
			"$(date "+%F | %R")"
		;;
	
	*)
		exit
		;;
esac

