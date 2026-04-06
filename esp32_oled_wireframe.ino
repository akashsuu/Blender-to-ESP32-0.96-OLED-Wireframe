/*
  ESP32 OLED Wireframe Receiver
  ==============================
  Receives 3D vertex + edge data from Blender over Serial (USB) or UDP (WiFi),
  projects it to 2D, and draws it as a wireframe on a 128x64 SSD1306 OLED.

  WIRING (SSD1306 OLED via I2C):
    OLED VCC  -> ESP32 3.3V
    OLED GND  -> ESP32 GND
    OLED SDA  -> ESP32 GPIO 21
    OLED SCL  -> ESP32 GPIO 22

  LIBRARIES (install via Arduino Library Manager):
    - Adafruit SSD1306  (by Adafruit)
    - Adafruit GFX      (by Adafruit)

  SERIAL MODE:   Set USE_SERIAL true  -- plug USB, done.
  UDP/WiFi MODE: Set USE_SERIAL false -- fill in WIFI_SSID / WIFI_PASSWORD.

  PACKET FORMAT (from Blender script, little-endian):
    Byte 0-1:   Magic 0xAB 0xCD
    Byte 2-3:   vert_count  (uint16)
    Byte 4-5:   edge_count  (uint16)
    Byte 6+:    vert_count x 12 bytes  (3x float32: x, y, z)
    Then:       edge_count  x 4  bytes (2x uint16:  a, b)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─────────────────────────────────────────────
//  USER SETTINGS
// ─────────────────────────────────────────────
#define USE_SERIAL    true          // true = USB Serial, false = WiFi UDP

const char* WIFI_SSID     = "YourSSID";
const char* WIFI_PASSWORD = "YourPassword";
const int   UDP_PORT      = 4210;

#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C          // Try 0x3D if screen stays blank

#define SERIAL_BAUD   115200
#define MAX_VERTS     128
#define MAX_EDGES     200

// Projection: scale and center the image on-screen
float SCALE_X  = 28.0f;
float SCALE_Y  = 18.0f;
float OFFSET_X = 64.0f;
float OFFSET_Y = 32.0f;

// Auto-spin (degrees per frame, set to 0 to disable)
float AUTO_ROTATE_Y = 0.0f;
float AUTO_ROTATE_X = 0.0f;
// ─────────────────────────────────────────────

#if !USE_SERIAL
  #include <WiFi.h>
  #include <WiFiUdp.h>
  WiFiUDP udp;
#endif

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

float    verts[MAX_VERTS][3];
uint16_t edges[MAX_EDGES][2];
int      vert_count = 0;
int      edge_count = 0;

float rot_y = 0.0f;
float rot_x = 0.0f;

#define RECV_BUF_SIZE 4096
uint8_t recv_buf[RECV_BUF_SIZE];
int     recv_len = 0;


// ══════════════════════════════════════════════
//  MATH
// ══════════════════════════════════════════════

inline float deg2rad(float d) { return d * 0.017453293f; }

void rotate_point(float ix, float iy, float iz,
                  float ry_deg, float rx_deg,
                  float &ox, float &oy, float &oz) {
  float ry = deg2rad(ry_deg);
  float rx = deg2rad(rx_deg);
  float x1 =  ix * cosf(ry) + iz * sinf(ry);
  float y1 =  iy;
  float z1 = -ix * sinf(ry) + iz * cosf(ry);
  ox =  x1;
  oy =  y1 * cosf(rx) - z1 * sinf(rx);
  oz =  y1 * sinf(rx) + z1 * cosf(rx);
}

void project(float wx, float wy, float wz, int16_t &sx, int16_t &sy) {
  float rx, ry, rz;
  rotate_point(wx, wy, wz, rot_y, rot_x, rx, ry, rz);
  sx = (int16_t)(OFFSET_X + rx * SCALE_X);
  sy = (int16_t)(OFFSET_Y - ry * SCALE_Y);
}


// ══════════════════════════════════════════════
//  PACKET PARSER
// ══════════════════════════════════════════════

bool parse_packet(uint8_t* buf, int len) {
  if (len < 6) return false;
  if (buf[0] != 0xAB || buf[1] != 0xCD) return false;

  uint16_t vc = buf[2] | (buf[3] << 8);
  uint16_t ec = buf[4] | (buf[5] << 8);

  if (vc > MAX_VERTS || ec > MAX_EDGES) return false;
  if (len < 6 + vc * 12 + ec * 4) return false;

  int off = 6;
  for (int i = 0; i < vc; i++, off += 12) {
    memcpy(&verts[i][0], buf + off,     4);
    memcpy(&verts[i][1], buf + off + 4, 4);
    memcpy(&verts[i][2], buf + off + 8, 4);
  }
  for (int i = 0; i < ec; i++, off += 4) {
    edges[i][0] = buf[off]     | (buf[off+1] << 8);
    edges[i][1] = buf[off + 2] | (buf[off+3] << 8);
  }

  vert_count = vc;
  edge_count = ec;
  return true;
}


// ══════════════════════════════════════════════
//  DRAW
// ══════════════════════════════════════════════

void draw_wireframe() {
  display.clearDisplay();
  for (int i = 0; i < edge_count; i++) {
    uint16_t a = edges[i][0];
    uint16_t b = edges[i][1];
    if (a >= (uint16_t)vert_count || b >= (uint16_t)vert_count) continue;

    int16_t ax, ay, bx, by;
    project(verts[a][0], verts[a][1], verts[a][2], ax, ay);
    project(verts[b][0], verts[b][1], verts[b][2], bx, by);

    bool a_vis = (ax >= 0 && ax < OLED_WIDTH  && ay >= 0 && ay < OLED_HEIGHT);
    bool b_vis = (bx >= 0 && bx < OLED_WIDTH  && by >= 0 && by < OLED_HEIGHT);
    if (a_vis || b_vis)
      display.drawLine(ax, ay, bx, by, WHITE);
  }
  display.display();
}


// ══════════════════════════════════════════════
//  SERIAL FRAME SYNC
// ══════════════════════════════════════════════

#if USE_SERIAL
bool try_read_serial() {
  while (Serial.available() && recv_len < RECV_BUF_SIZE)
    recv_buf[recv_len++] = Serial.read();

  for (int i = 0; i <= recv_len - 6; i++) {
    if (recv_buf[i] == 0xAB && recv_buf[i+1] == 0xCD) {
      uint16_t vc = recv_buf[i+2] | (recv_buf[i+3] << 8);
      uint16_t ec = recv_buf[i+4] | (recv_buf[i+5] << 8);
      if (vc > MAX_VERTS || ec > MAX_EDGES) { recv_len = 0; return false; }

      int expected = 6 + vc * 12 + ec * 4;
      if (recv_len - i < expected) return false;   // Wait for more bytes

      bool ok      = parse_packet(recv_buf + i, expected);
      int  remain  = recv_len - i - expected;
      if (remain > 0) memmove(recv_buf, recv_buf + i + expected, remain);
      recv_len = remain;
      return ok;
    }
  }
  // No magic found — discard everything older than 6 bytes
  if (recv_len > 6) {
    memmove(recv_buf, recv_buf + recv_len - 6, 6);
    recv_len = 6;
  }
  return false;
}
#endif


// ══════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════

void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERROR: SSD1306 not found. Check wiring + I2C address.");
    for (;;);
  }

  // Splash
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(4, 8);  display.print("Wireframe Receiver");
  display.setCursor(4, 22);
#if USE_SERIAL
  display.print("Serial USB mode");
  display.setCursor(4, 34); display.print("Waiting for Blender");
#else
  display.print("WiFi UDP mode");
  display.setCursor(4, 34); display.print("Connecting...");
#endif
  display.display();

#if !USE_SERIAL
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 40) {
    delay(500); Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 0); display.print("WiFi FAILED");
    display.display();
    for (;;);
  }

  udp.begin(UDP_PORT);
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 0);  display.print("WiFi OK");
  display.setCursor(0, 12); display.print(WiFi.localIP().toString());
  display.setCursor(0, 24); display.printf("UDP :%d", UDP_PORT);
  display.display();
  delay(2000);
#else
  delay(1200);
#endif

  display.clearDisplay();
  display.display();
}


// ══════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════

void loop() {
  bool new_frame = false;

#if USE_SERIAL
  new_frame = try_read_serial();
#else
  int pkt = udp.parsePacket();
  if (pkt > 0 && pkt <= RECV_BUF_SIZE) {
    int n  = udp.read(recv_buf, RECV_BUF_SIZE);
    new_frame = parse_packet(recv_buf, n);
  }
#endif

  bool spinning = (AUTO_ROTATE_Y != 0.0f || AUTO_ROTATE_X != 0.0f);
  if (spinning) {
    rot_y += AUTO_ROTATE_Y;
    rot_x += AUTO_ROTATE_X;
    if (rot_y >= 360.0f) rot_y -= 360.0f;
    if (rot_x >= 360.0f) rot_x -= 360.0f;
    draw_wireframe();
  } else if (new_frame) {
    draw_wireframe();
  }
}
