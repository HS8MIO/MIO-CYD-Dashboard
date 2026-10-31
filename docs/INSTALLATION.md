# Installation guide

This guide adds the MIO CYD Dashboard to an **already installed and working Pi-Star system**. It does not install Pi-Star and does not provide a Pi-Star disk image.

## 1. Prepare and verify Pi-Star

1. Install Pi-Star using the normal method for your Raspberry Pi.
2. Configure your MMDVM modem and D-Star/DMR services as required.
3. Confirm that Pi-Star is operating normally before continuing.
4. Back up your SD card or otherwise make a recovery copy before changing system files.

The Stable 1.0 verification baseline used Raspberry Pi 4 Model B and Pi-Star 4.2.3.

## 2. Prepare the CYD

Recommended initial hardware: ESP32 CYD 2.8-inch, 320×240 (ESP32-2432S028 family).

The supplied firmware uses the TFT_eSPI library. Its selected TFT_eSPI setup must match this display configuration:

- Controller: ILI9341
- MOSI: GPIO 13
- MISO: GPIO 12
- SCLK: GPIO 14
- CS: GPIO 15
- DC: GPIO 2
- RST: -1
- Backlight: GPIO 21, active HIGH
- Rotation: landscape, setRotation(1)

The board's USB connection provides the serial connection to the Raspberry Pi. Use a USB data cable, not a charge-only cable.

### Upload the firmware

1. Install Arduino IDE and the ESP32 board support package.
2. Install the TFT_eSPI library.
3. Check that the TFT_eSPI setup selected by `User_Setup_Select.h` matches the pin/controller configuration above.
4. Open `CYD/CYD_28_MMDVM_Serial_Test_UI_Time_IP.ino`.
5. Select the appropriate ESP32 board and the CYD's USB serial port.
6. Compile and upload.
7. The firmware communicates at **115200 baud**.

The initial baseline was verified on the ESP32 CYD 2.8-inch board. ESP32-S3 2.8-inch boards are not certified by this baseline; their display controller, pinout, and TFT_eSPI setup may differ.

## 3. Connect the CYD to Pi-Star

1. Connect the CYD to a USB port on the Raspberry Pi.
2. SSH into Pi-Star.
3. Identify the serial device:

```sh
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

The verified baseline used `/dev/ttyUSB0` (CH340/CH341 USB serial). Your device name may be different.

## 4. Copy the Pi-Star files

Copy these two repository files to the Pi-Star user's home directory (for example, with SCP or an SFTP client):

- `Pi-Star/pistar_cyd_bridge.py`
- `Pi-Star/pistar-cyd-bridge.service`

Then connect to Pi-Star over SSH and make the filesystem writable:

```sh
rpi-rw
```

Install the files:

```sh
sudo install -m 755 ~/pistar_cyd_bridge.py /usr/local/sbin/pistar_cyd_bridge.py
sudo install -m 644 ~/pistar-cyd-bridge.service /etc/systemd/system/pistar-cyd-bridge.service
```

If your CYD appears as a device other than `/dev/ttyUSB0`, edit the service file before installing it. Change the device at the end of the `ExecStart=` line to the serial device you identified.

Reload systemd and start the bridge:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now pistar-cyd-bridge.service
sudo systemctl status pistar-cyd-bridge.service --no-pager
```

Check the service log if needed:

```sh
sudo journalctl -u pistar-cyd-bridge.service -n 50 --no-pager
```

When installation and testing are complete, return Pi-Star to read-only mode:

```sh
rpi-ro
```

Verify the root filesystem is read-only if that is your normal Pi-Star operating mode:

```sh
findmnt -no SOURCE,FSTYPE,OPTIONS /
```

## 5. Verify operation

With Pi-Star running and the CYD connected:

- The CYD should show the MMDVM dashboard layout.
- Callsign and mode should update when a D-Star/DMR header is received.
- TX state should show activity and return to Stand by when transmission ends.
- Link status should reflect the D-Star link state.
- LAST RX should show the last received station or slow-data text.
- Time and IP are supplied by the Pi-Star bridge.

The baseline was tested with D-Star links REF087 and XLX568 C.

## How it works

The Python bridge follows Pi-Star MMDVM log files at `/var/log/pi-star/MMDVM-*.log`, parses activity, and sends newline-delimited key/value updates to the CYD over USB serial. It does not open the MMDVM modem port and does not change Pi-Star configuration.

## Troubleshooting

- **No serial device:** Check the USB data cable and run the device listing command again. The device may be `/dev/ttyUSB1` or `/dev/ttyACM0`.
- **Service cannot open the port:** Confirm that `ExecStart=` in the service file names the correct serial device.
- **Dashboard does not update:** Check `systemctl status` and `journalctl`, confirm Pi-Star is producing MMDVM logs, and confirm the CYD firmware is running at 115200 baud.
- **Backlight but no display image:** Recheck the TFT_eSPI controller and pin configuration for the exact CYD board.

## Scope and safety

This is an initial community guide. Back up Pi-Star before installation. Keep a copy of the original system files so you can remove the bridge and restore your previous state if needed. Hardware variants beyond the tested CYD baseline may require changes.
