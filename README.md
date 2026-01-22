# unikey
Universal Keyboard: Wireless Device Interface

## Dependencies
* bluez
* bluez-tools
* build-essential
* cmake
* libevdev2
* pkg-config

### Install Required Packages
```bash 
sudo apt install libbluetooth-dev libsdbus-c++-dev libevdev-dev libudev-dev
```
## Compile The Project
```bash
cmake -S . -B bin -DCMAKE_BUILD_TYPE=Release && cmake --build bin
```
or using ninja as the generator
```bash
cmake -S . -B bin -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build bin
```
## Allow the Executable To Run as Input Group
This allows the binary to change its group ID to 'Input' so that it can be run without requiring sudo permissions.
```bash
sudo setcap "cap_setgid=eip" ./bin/unikey
```
This must be run every time the program is compiled

## Set System D-Bus Access Permissions For Unikey
```bash
sudo cp ./files/io.unikey.conf /etc/dbus-1/system.d/
sudo systemctl reload dbus.service
```

~/Projects/Test-Directory/bluetooth/bluetooth_scanner/build
/sys/bus/hid/devices
