
```
Move object in Blender  →  Python script extracts vertices + edges
→  Sent over Serial/UDP  →  ESP32 projects to 2D  →  OLED draws wireframe
```

---

## Hardware Required

| Part | Notes |
|------|-------|
| ESP32 WROOM-32 (38-pin) | Any ESP32 Dev Module works |
| SSD1306 OLED 128×64 | I2C version (4-pin) |
| USB cable | Must support data transfer, not charge-only |
| Jumper wires | 4 wires for I2C |

### Wiring

```
OLED VCC  →  ESP32 3.3V    (NOT 5V)
OLED GND  →  ESP32 GND
OLED SDA  →  ESP32 GPIO 21
OLED SCL  →  ESP32 GPIO 22
```

---

## Setup

### 1. Flash the ESP32

- Open `esp32_oled_wireframe/esp32_oled_wireframe.ino` in Arduino IDE
- Install libraries via Library Manager: **Adafruit SSD1306** and **Adafruit GFX**
- Set `Tools → Board → ESP32 Dev Module`
- Select your COM port under `Tools → Port`
- Click Upload (hold the BOOT button when you see `Connecting.....`)

### 2. Install pyserial into Blender

Blender uses its own Python — paste this in Blender's **Python Console**:

```python
import subprocess, sys
subprocess.check_call([
    sys.executable, "-m", "pip", "install",
    "--target",
    r"C:\Users\YOUR_USERNAME\AppData\Roaming\Blender Foundation\Blender\4.5\extensions\.local\lib\python3.11\site-packages",
    "pyserial"
])
```

Replace `YOUR_USERNAME` with your Windows username. On Mac/Linux the path will differ — check `sys.path` in the console to find the right target folder.

### 3. Run the Blender script

- Open Blender → Scripting workspace
- Open `blender_wireframe_sender.py` and click **Run Script**
- Switch to 3D Viewport → press `N` → open **Wireframe Sender** tab
- Set your COM port (e.g. `COM5` on Windows, `/dev/ttyUSB0` on Linux)
- Select your mesh object
- Click **▶ Start Sending**

---

## Features

- Live streaming over USB Serial or WiFi UDP
- Works in Object Mode and Edit Mode (vertex/edge/face edits update in real time)
- Auto-decimation if mesh exceeds vertex/edge limits
- Orthographic or perspective projection (configurable)
- Optional auto-rotation on the OLED
- N-panel UI inside Blender's 3D Viewport

---

## Configuration

### ESP32 (`esp32_oled_wireframe.ino`)

```cpp
#define USE_SERIAL    true     // true = USB, false = WiFi UDP
float SCALE_X       = 28.0f;  // Zoom — increase to make wireframe bigger
float SCALE_Y       = 18.0f;
float AUTO_ROTATE_Y = 0.0f;   // Auto-spin (degrees per frame, 0 = off)
#define PERSPECTIVE_D 0.0f    // 0 = orthographic, 3.0 = perspective
```

### Blender script

Edit these at the top of `blender_wireframe_sender.py`:

```python
DEFAULT_PORT   = "COM5"          # Your serial port
DEFAULT_UDP_IP = "192.168.1.100" # ESP32 IP (UDP mode only)
MAX_VERTS      = 128             # Keep low for performance
MAX_EDGES      = 200
SEND_FPS       = 20
```

---

## Troubleshooting

See **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** for a full list of errors and fixes, including:

- `Wrong boot mode detected` — ESP32 not in download mode
- `No serial data received` — charge-only USB cable
- `ModuleNotFoundError: No module named 'serial'` — pyserial in wrong Python
- OLED shows only a square — projection and scale fixes
- Edit Mode not updating live — dependency graph fix

---

## File Structure

```
├── blender_wireframe_sender.py        # Blender addon script
├── esp32_oled_wireframe/
│   └── esp32_oled_wireframe.ino      # ESP32 Arduino firmware
├── TROUBLESHOOTING.md                 # Error reference guide
└── README.md
```

---

## How It Works

1. A Blender modal operator runs every frame and reads the active mesh
2. Vertices are transformed to world space using the object's matrix
3. A compact binary packet is built: `[magic][vert_count][edge_count][xyz...][ab...]`
4. The packet is sent over Serial (USB) or UDP (WiFi) to the ESP32
5. The ESP32 parses the packet, applies optional rotation, projects 3D → 2D
6. Lines are drawn between edge pairs using Bresenham's algorithm via Adafruit GFX
7. The frame buffer is pushed to the SSD1306 via I2C

---

## License

MIT
