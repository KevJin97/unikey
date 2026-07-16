# Define Some Variables For Convenience

set(PROJECT_SOURCES
	src/unikey.cpp
	src/Bluetooth/unikey-bluetooth.cpp
	src/WiFi/unikey-wifi.cpp
	src/Bluetooth/BlueZ_Agent.cpp
	src/Bluetooth/BlueZ_Interface.cpp
	src/Bluetooth/Gatt/Base_App_Obj.cpp
	src/Bluetooth/Gatt/Characteristic.cpp
	src/Bluetooth/Gatt/Descriptor.cpp
	src/Bluetooth/Gatt/Service.cpp
	src/Core/BitField.cpp
	src/Core/Cyclic_Queue.cpp
	src/Core/Device.cpp
	src/WiFi/Virtual_Device.cpp
	src/WiFi/WiFi_Client.cpp
	src/WiFi/WiFi_Server.cpp
)
set(PROJECT_INCLUDE_DIRS
	${LIBEVDEV_INCLUDE_DIRS}
	${SDBUSCPP_INCLUDE_DIRS}
	${LIBUDEV_INCLUDE_DIRS}
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set(PROJECT_LIBRARIES
	${LIBEVDEV_LIBRARIES}
	${SDBUSCPP_LIBRARIES}
	${LIBUDEV_LIBRARIES}
)

set(PROJECT_EXEC_NAME ${PROJECT_NAME})
set(PROJECT_LIB_NAME "lib${PROJECT_NAME}")