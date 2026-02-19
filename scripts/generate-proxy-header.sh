#!/bin/bash

gdbus introspect --system --dest org.bluez --object-path /org/bluez/hci0 --xml > ../files/org_bluez_proxy.xml

sdbus-c++-xml2cpp ../files/org_bluez_proxy.xml --proxy=org_bluez_proxy.hpp
sdbus-c++-xml2cpp ../files/org_bluez_agent_interface.xml --proxy=org_bluez_agent_interface.hpp

mv ./*.hpp ../include/Bluetooth/