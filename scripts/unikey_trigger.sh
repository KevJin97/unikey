#!/bin/bash

busctl --system call io.unikey.Device \
	/io/unikey/Device \
	io.unikey.Device.Methods \
	Trigger
