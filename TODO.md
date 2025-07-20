Try replacing Watchdog queue handler for a semaphore.

If file descriptor == -ENODEV, it has been disconnected

EV_REL
- codes: REL_X, REL_Y, REL_WHEEL, REL_HWHEEL
- value: signed integer



EV_ABS
- codes: ABS_X, ABS_Y, ABS_Z, ABS_PRESSURE, ABS_MT_SLOT(*)
- value: signed integer

*Multi-touch protocols
- ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_PRESSURE

Example of Single-Touch:
	EV_ABS ABS_X  [current_x_coord]
	EV_ABS ABS_Y  [current_y_coord]
	EV_ABS ABS_PRESSURE [current_pressure]
	EV_SYN SYN_REPORT 0

Example of Double-Touch:
	EV_ABS ABS_MT_SLOT 1
	EV_ABS ABS_MT_POSITION_X [x2]
	EV_ABS ABS_MT_POSITION_Y [y2]
	EV_ABS ABS_MT_PRESSURE [p2]

Get EV_ABS information using
	libevdev_get_abs_info(dev, code)
Use the information field
	struct input_absinfo
which has fields
- minimum
- maximum
- fuzz
- flat
- resolution




Required fields:
	libevdev_get_name(dev)
	libevdev_get_id_bustype(dev)
	libevdev_get_id_product(dev)
	libevdev_get_id_version(dev)

For EV_ABS:
	struct input_absinfo* abs_info = libevdev_get_info(dev, code)
	libevdev_enable_event_code(uinput_dev, EV_ABS, code, abs_info)