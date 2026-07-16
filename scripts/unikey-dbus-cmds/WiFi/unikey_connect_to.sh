#!/bin/bash

busctl --system call io.unikey \
	/io/unikey/WiFi \
	io.unikey.WiFi.Methods \
	WiFiConnectTo s $1
