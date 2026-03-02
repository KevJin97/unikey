#!/bin/bash

busctl --system call io.unikey \
	/io/unikey/Bluetooth \
	io.unikey.Bluetooth.Methods \
	SetDisplayName s $1
