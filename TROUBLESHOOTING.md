# Troubleshooting Guide — Blender → ESP32 OLED Wireframe

A complete reference of every error you might hit and exactly how to fix it.

---

## Table of Contents

1. [ESP32 Upload Errors](#esp32-upload-errors)
2. [Blender / pyserial Errors](#blender--pyserial-errors)
3. [OLED Display Issues](#oled-display-issues)
4. [Live Edit Mode Not Updating](#live-edit-mode-not-updating)

---

## ESP32 Upload Errors

---

### ❌ Wrong boot mode detected (0x13)

```
A fatal error occurred: Failed to connect to ESP32: Wrong boot mode detected (0x13)!
The chip needs to be in download mode.
```

**Cause:** The ESP32 was in normal run mode when Arduino IDE tried to upload. It missed the bootloader window.

**Fix:**
1. Click **Upload** in Arduino IDE
2. Watch the console — the moment you see `Connecting.........` appear, press and hold the **BOOT** button on the ESP32
3. Hold until you see `Writing at 0x00001000...`
4. Release BOOT, then press **EN** once to reboot into your sketch

> The BOOT button is usually labeled **BOOT** or **IO0** on the board.

---

### ❌ No serial data received

```
A fatal error occurred: Failed to connect to ESP32: No serial data received.
```

**Cause:** The chip isn't responding at all — usually a charge-only USB cable or wrong boot sequence timing.

**Fix — try in this order:**

**Option A — Charge-only USB cable (most common cause)**
Swap your USB cable for one you know transfers data (e.g. a phone cable that syncs files). Many micro-USB cables are charge-only and show up in Device Manager fine but silently drop all data.

**Option B — Full manual boot sequence**
1. Press and hold **BOOT**
2. While still holding BOOT, press and release **EN**
3. Keep holding BOOT — now click **Upload** in Arduino IDE
4. Release BOOT only after you see `Writing at 0x00001000...`

**Option C — Lower upload speed**
In Arduino IDE: `Tools → Upload Speed → 115200` (default 921600 is too fast for some boards), then retry.

**Option D — Wrong board selected**
`Tools → Board` must be set to **ESP32 Dev Module** — not ESP32-S2, not ESP8266, not NodeMCU.

---

### ❌ Port not detected / COM port missing

**Cause:** USB-Serial driver not installed. ESP32 boards use a CP2102 or CH340 chip that Windows/Mac doesn't include drivers for by default.

**Fix:**

Check what chip your board uses (look at the small IC near the USB port), then install the matching driver:

| Chip | Driver |
|------|--------|
| CP2102 / CP2104 | Silicon Labs CP210x |
| CH340 / CH341 | WCH CH340 |
| FT232 | FTDI VCP |

After installing: unplug → replug USB → check Device Manager again.

**Windows:** Device Manager → Ports (COM & LPT). Look for CP210x, CH340, or USB Serial Device.

**Mac:** Run `ls /dev/cu.*` in Terminal. Look for `/dev/cu.usbserial-XXXX` or `/dev/cu.SLAB_USBtoUART`.

**Linux:** Run `ls /dev/ttyUSB* /dev/ttyACM*`. Usually `/dev/ttyUSB0`. Also add yourself to the dialout group:
```bash
sudo usermod -a -G dialout $USER
# Log out and back in after this
```

---

### ❌ SSD1306 not found / OLED stays blank after upload

```
ERROR: SSD1306 not found — check wiring and I2C address
```

**Cause:** OLED wiring issue or wrong I2C address.

**Fix — check wiring first:**
```
OLED VCC  →  ESP32 3.3V   (NOT 5V — will damage the display)
OLED GND  →  ESP32 GND
OLED SDA  →  ESP32 GPIO 21
OLED SCL  →  ESP32 GPIO 22
```

**Fix — try alternate I2C address:**
In `esp32_oled_wireframe.ino`, change:
```cpp
#define OLED_ADDRESS  0x3C
```
to:
```cpp
#define OLED_ADDRESS  0x3D
```
Some SSD1306 modules ship with `0x3D` instead of `0x3C`.

---

## Blender / pyserial Errors

---

### ❌ Could not open connection — check port/IP in the panel

**Cause:** pyserial is not installed in Blender's Python, or another program is holding the port.

**Fix A — Install pyserial into Blender's Python**

Blender ships with its own isolated Python. Installing pyserial with your system `pip` does not affect Blender. You must install it through Blender's Python Console:

Open the **Python Console** editor in Blender and paste:
```python
import subprocess, sys
subprocess.check_call([
    sys.executable, "-m", "pip", "install",
    "--target",
    r"C:\Users\YOUR_USERNAME\AppData\Roaming\Blender Foundation\Blender\4.5\extensions\.local\lib\python3.11\site-packages",
    "pyserial"
])
```
Replace `YOUR_USERNAME` with your actual Windows username. Then verify:
```python
import serial
print("OK:", serial.__version__)
```
If it prints a version number, pyserial is ready.

**Fix B — Close Arduino IDE**
Only one program can hold a serial port at a time. Close Arduino IDE (especially its Serial Monitor) before running the Blender script.

---

### ❌ ModuleNotFoundError: No module named 'serial'

**Cause:** pyserial installed to system Python, not Blender's Python.

**Diagnosis:** Run in Blender's Python Console:
```python
import sys
print(sys.executable)   # Should show Blender's python.exe path
for p in sys.path:
    print(p)
```

**Fix:** Use the `--target` flag to install directly into a folder that's in Blender's `sys.path` (see fix above). The correct target path will be visible in the `sys.path` output — look for a path containing `extensions\.local\lib\python3.11\site-packages`.

---

### ❌ Permission error installing to Program Files

```
subprocess.CalledProcessError: Command returned non-zero exit status 2
```

**Cause:** `C:\Program Files\` is write-protected. pip can't install there without admin rights.

**Fix:** Install to the user AppData folder instead (which Blender also checks):
```python
import subprocess, sys
subprocess.check_call([
    sys.executable, "-m", "pip", "install",
    "--target",
    r"C:\Users\YOUR_USERNAME\AppData\Roaming\Blender Foundation\Blender\4.5\extensions\.local\lib\python3.11\site-packages",
    "pyserial"
])
```

---

### ❌ OLED shows only a square / looks flat

**Cause:** A cube viewed perfectly front-on looks identical to a square in orthographic projection.

**Fix A — Enable auto-rotation** in `esp32_oled_wireframe.ino`:
```cpp
float AUTO_ROTATE_Y = 0.5f;   // Slow spin around Y axis
float AUTO_ROTATE_X = 0.2f;   // Slight tilt on X axis
```

**Fix B — Enable perspective projection:**
```cpp
#define PERSPECTIVE_D  3.0f   // 0 = orthographic, 3 = mild perspective
```

**Fix C — Scale up the image:**
```cpp
float SCALE_X = 45.0f;   // default was 28.0
float SCALE_Y = 30.0f;   // default was 18.0
```

Re-upload the `.ino` after any change.

---

## Live Edit Mode Not Updating

### ❌ OLED doesn't update while editing vertices in Tab mode

**Cause:** The script reads `obj.data` which only reflects the saved mesh state, not the live edit.

**Fix:** In `blender_wireframe_sender.py`, inside the `get_wireframe_data` function, replace:
```python
mesh = obj.data
mat  = obj.matrix_world
```
with:
```python
depsgraph = bpy.context.evaluated_depsgraph_get()
obj_eval  = obj.evaluated_get(depsgraph)
mesh      = obj_eval.data
mat       = obj.matrix_world
```

This uses Blender's dependency graph to get the fully evaluated mesh, which updates live as you move vertices, extrude faces, add loop cuts, and apply modifiers — all in real time.

---

## Quick Checklist

Before opening an issue, verify:

- [ ] USB cable supports data transfer (not charge-only)
- [ ] Arduino IDE and its Serial Monitor are fully closed
- [ ] Board set to **ESP32 Dev Module** in Arduino IDE
- [ ] COM port matches what Device Manager shows
- [ ] pyserial installed via Blender's Python Console (not system pip)
- [ ] OLED wired to 3.3V, not 5V
- [ ] SSD1306 I2C address tried as both `0x3C` and `0x3D`
- [ ] Mesh object is selected in Blender before clicking Start Sending

---

## Still stuck?

Open an issue and include:
- Your OS and Blender version
- The exact error message from the console
- Output of this command run in Blender's Python Console:
```python
import sys
print(sys.version)
print(sys.executable)
print(sys.path)
```
