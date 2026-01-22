#!/bin/bash

busctl --system call io.unikey \
	/io/unikey/Device \
	io.unikey.Device.Methods \
	SetTimeout u $1
