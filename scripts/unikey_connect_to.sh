#!/bin/bash

busctl --system call io.unikey \
	/io/unikey/WiFi \
	io.unikey.WiFi.Methods \
	ConnectTo s $1
