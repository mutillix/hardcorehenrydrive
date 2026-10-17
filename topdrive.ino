// HARDCORE HENRY DRIVE — top-down GPS-speed driver for the Elecrow ES3C35P (ESP32-S3)
// Portrait 320x480. Road scrolls at real GPS speed. Parked = parked.
// TAP BOOT = change lane. LONG-PRESS BOOT = swap vehicle (FIT / CLASS C RV).
// No GPS fix -> gentle auto-cruise.
// GPS: NEO-6M on RX45 / TX46, 9600 baud.
// SD: 4-bit SDIO on CLK=5 CMD=4 D0=6 D1=7 D2=2 D3=3. Logs GPS locations + speed.
// 2026-10-16: optimized draw, full RMC parse (lat/lon/time/speed), SD CSV log,
//   Fit/RV only, higher-detail GBA-style pixel cars matching Hardcore Henry aesthetic.
// 2026-10-16 (Lou merge): BLE scan spawns roadside items from real radios —
//   named BLE devices become billboards (name on the sign), unnamed become pylons,
//   panel color heat-mapped by RSSI. Poster art boots as the splash (topdrive_splash.h).
// IDE: ESP32S3 Dev Module, Flash Size 8MB, PSRAM OPI PSRAM,
//   Partition Scheme "8M with spiffs (3MB APP/1.5MB SPIFFS)" (BLE stack + splash
//   push the sketch past the 1.25MB default app partition).
// Libraries: esp_display_panel (proven ST77922 init).
// ============================================================

#include <Arduino.h>
#include <esp_display_panel.hpp>
using namespace esp_panel::drivers;
#include <HardwareSerial.h>
#include "FS.h"
#include "SD_MMC.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <atomic>
#include "topdrive_splash.h"

#define LCD_CS 10
#define LCD_SCK 12
#define LCD_D0 11
#define LCD_D1 13
#define LCD_D2 14
#define LCD_D3 9
#define LCD_BL 41
#define PIN_BOOT 0

#define SCR_W 320
#define SCR_H 480

BusQSPI *bus = new BusQSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
LCD *lcd = new LCD_ST77922(bus, 320, 480, 16, -1);

// Elecrow ES3C35P ST77922 init — proven from ble_radar (includes SLPOUT/DISPON/INVON)
const esp_panel_lcd_vendor_init_cmd_t vendor_init[] = {
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xF1, {0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x60, {0x00, 0x00, 0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x65, {0x80}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x79, {0x06}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x7B, {0x00, 0x08, 0x08}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x80, {0x55, 0x62, 0x2F, 0x17, 0xF0, 0x52, 0x70, 0xD2, 0x52, 0x62, 0xEA}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x81, {0x26, 0x52, 0x72, 0x27}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x84, {0x92, 0x25}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x87, {0x10, 0x10, 0x58, 0x00, 0x02, 0x3A}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x88, {0x00, 0x00, 0x2C, 0x10, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x06}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x89, {0x00, 0x00, 0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x8A, {0x13, 0x00, 0x2C, 0x00, 0x00, 0x2C, 0x10, 0x10, 0x00, 0x3E, 0x19}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x8B, {0x15, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x97, 0x8E}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x8C, {0x1D, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x50, 0x0F, 0x01, 0xC5, 0x12, 0x09}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x8D, {0x0C}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x8E, {0x33, 0x01, 0x0C, 0x13, 0x01, 0x01}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xB3, {0x00, 0x30}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xF1, {0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x71, {0xC0}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x66, {0x02, 0x3F}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xBE, {0x24, 0x00, 0x9D}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x70, {0x01, 0xA0, 0x11, 0x40, 0xE0, 0x00, 0x11, 0x69, 0x11, 0x00, 0x00, 0x1A}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x90, {0x04, 0x04, 0x55, 0x74, 0x00, 0x40, 0x43, 0x27, 0x27}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x91, {0x04, 0x04, 0x55, 0x75, 0x00, 0x40, 0x42, 0x27, 0x27}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x92, {0x04, 0x44, 0x55, 0xC0, 0x06, 0x00, 0x07, 0x05, 0x90, 0x27}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x93, {0x04, 0x43, 0x11, 0x00, 0x00, 0x00, 0x00, 0x05, 0x90, 0x27}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x94, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x95, {0x96, 0x16, 0x00, 0x00, 0xFF}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x96, {0x44, 0x53, 0x03, 0x12, 0x23, 0x24, 0x06, 0x05, 0x94, 0x27, 0x00, 0x44}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x97, {0x44, 0x53, 0x47, 0x56, 0x20, 0x20, 0x02, 0x01, 0x94, 0x27, 0x00, 0x44}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xBA, {0x55, 0x94, 0x2D, 0x94, 0x27}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x9A, {0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x9B, {0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x9C, {0x5C, 0x12, 0x00, 0x00, 0x10, 0x12, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x9D, {0x8A, 0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x9E, {0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xB4, {0x1D, 0x1C, 0x1E, 0x0B, 0x14, 0x02, 0x13, 0x09, 0x1E, 0x00, 0x1E, 0x10}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xB5, {0x1D, 0x1C, 0x1E, 0x0A, 0x15, 0x03, 0x11, 0x08, 0x1E, 0x01, 0x1E, 0x12}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xB6, {0x77, 0x77, 0x00, 0x0A, 0xFF, 0x0A, 0xFF}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x86, {0xCD, 0x04, 0xB1, 0x02, 0x58, 0x12, 0x58, 0x0C, 0x13, 0x01, 0xA5, 0x00, 0xA5, 0xA5}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xB7, {0x07, 0x0A, 0x0E, 0x06, 0x05, 0x03, 0x2B, 0x03, 0x03, 0x42, 0x07, 0x10, 0x10, 0x2E, 0x3F, 0x0D}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xB8, {0x07, 0x0A, 0x0D, 0x05, 0x05, 0x02, 0x2B, 0x02, 0x03, 0x42, 0x06, 0x10, 0x0F, 0x2E, 0x3F, 0x0D}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xB9, {0x23, 0x23}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xBF, {0x10, 0x14, 0x14, 0x0B, 0x0B, 0x0B}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xF2, {0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x73, {0x04, 0xDA, 0x12, 0x54, 0x47}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x77, {0x6B, 0x5B, 0xFD, 0xC3, 0xC5}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x7A, {0x15, 0x27}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x7B, {0x04, 0x57}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0x7E, {0x01, 0x0E}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xBF, {0x36}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xE3, {0x40, 0x40}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xF0, {0x00}),
ESP_PANEL_LCD_CMD_WITH_8BIT_PARAM(0, 0xD0, {0x00}),
ESP_PANEL_LCD_CMD_WITH_NONE_PARAM(120, 0x11), // SLPOUT
ESP_PANEL_LCD_CMD_WITH_NONE_PARAM(0, 0x29), // DISPON
ESP_PANEL_LCD_CMD_WITH_NONE_PARAM(0, 0x21), // INVON
};

// ---- road ----
#define ROAD_W 204
#define ROAD_X ((SCR_W - ROAD_W) / 2)
#define LANES 3
#define LANE_W (ROAD_W / LANES)
#define CAR_Y (SCR_H - 130)
static int vehicle = 0;  // 0 = Honda Fit, 1 = Class C RV
static const char *VEH_NAMES[] = {"FIT", "RV"};
static int vehHalfW() { return vehicle == 1 ? 18 : 16; }
static int vehHalfH() { return vehicle == 1 ? 42 : 28; }

// ---- framebuffer ----
static uint8_t *fb;
static inline void setPx(int x, int y, uint16_t c) {
  if ((unsigned)x >= SCR_W || (unsigned)y >= SCR_H) return;
  uint8_t *p = &fb[(y * SCR_W + x) * 2];
  p[0] = c >> 8; p[1] = c & 0xFF;
}
static void fillRect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (y + h > SCR_H) h = SCR_H - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++) {
    uint8_t *p = &fb[((y + j) * SCR_W + x) * 2];
    for (int i = 0; i < w; i++) { p[0] = c >> 8; p[1] = c & 0xFF; p += 2; }
  }
}
static inline uint16_t rgb565(int r, int g, int b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static void fillCircle(int cx, int cy, int r, uint16_t c) {
  int r2 = r * r;
  for (int yy = -r; yy <= r; yy++) {
    int w = (int)sqrtf(r2 - yy * yy);
    fillRect(cx - w, cy + yy, w * 2 + 1, 1, c);
  }
}

// ---- tiny 5x7 font ----
static const uint8_t F5X7[][6] = {
  {'0', 0x3E,0x51,0x49,0x45,0x3E}, {'1', 0x00,0x42,0x7F,0x40,0x00},
  {'2', 0x42,0x61,0x51,0x49,0x46}, {'3', 0x21,0x41,0x45,0x4B,0x31},
  {'4', 0x18,0x14,0x12,0x7F,0x10}, {'5', 0x27,0x45,0x45,0x45,0x39},
  {'6', 0x3C,0x4A,0x49,0x49,0x30}, {'7', 0x01,0x71,0x09,0x05,0x03},
  {'8', 0x36,0x49,0x49,0x49,0x36}, {'9', 0x06,0x49,0x49,0x29,0x1E},
  {'K', 0x7F,0x08,0x14,0x22,0x41}, {'M', 0x7F,0x02,0x0C,0x02,0x7F},
  {'H', 0x7F,0x08,0x08,0x08,0x7F}, {'G', 0x3E,0x41,0x49,0x49,0x7A},
  {'P', 0x7F,0x09,0x09,0x09,0x06}, {'S', 0x46,0x49,0x49,0x49,0x31},
  {'/', 0x20,0x10,0x08,0x04,0x02}, {'.', 0x00,0x60,0x60,0x00,0x00},
  {' ', 0x00,0x00,0x00,0x00,0x00}, {':', 0x00,0x36,0x36,0x00,0x00},
  {'-', 0x08,0x08,0x08,0x08,0x08}, {'R', 0x7F,0x09,0x19,0x29,0x46},
  {'E', 0x7F,0x49,0x49,0x49,0x41}, {'C', 0x3E,0x41,0x41,0x41,0x22},
  {'V', 0x1F,0x20,0x40,0x20,0x1F}, {'F', 0x7F,0x09,0x09,0x09,0x01},
  {'I', 0x00,0x41,0x7F,0x41,0x00}, {'T', 0x01,0x01,0x7F,0x01,0x01},
  {'L', 0x7F,0x40,0x40,0x40,0x40}, {'N', 0x7F,0x04,0x08,0x10,0x7F},
};
static const uint8_t *glyphFor(char c) {
  for (unsigned i = 0; i < sizeof(F5X7) / sizeof(F5X7[0]); i++)
    if (F5X7[i][0] == (uint8_t)c) return &F5X7[i][1];
  return &F5X7[16][1];
}
static void drawText(int x, int y, const char *s, uint16_t fg, int sc) {
  while (*s) {
    const uint8_t *g = glyphFor(*s);
    for (int col = 0; col < 5; col++)
      for (int row = 0; row < 7; row++)
        if (g[col] & (1 << row)) fillRect(x + col * sc, y + row * sc, sc, sc, fg);
    x += 6 * sc;
    s++;
  }
}

// ---- GPS ----
static HardwareSerial GPSSerial(2);
static char gpsLine[96];
static int gpsLen = 0;
static bool gpsFix = false;
static float gpsMphSmooth = 0;
static uint32_t gpsLast = 0;
static float gpsLat = 0, gpsLon = 0;
static char gpsTime[16] = "--:--:--";
static char gpsDate[12] = "--/--/--";
static bool sdOk = false;
static uint32_t lastLog = 0;

static bool nmeaOk(char *l) {
  char *star = strchr(l, '*');
  if (!star) return true;
  uint8_t cs = 0;
  for (char *p = l + 1; p < star; p++) cs ^= *p;
  unsigned int sent = 0;
  if (sscanf(star + 1, "%02x", &sent) != 1) return false;
  return (uint8_t)sent == cs;
}
static float nmeaToDec(const char *s, char hemi) {
  if (!s || !*s) return 0;
  float v = atof(s);
  int deg = (int)(v / 100);
  float min = v - deg * 100;
  float dec = deg + min / 60.0f;
  if (hemi == 'S' || hemi == 'W') dec = -dec;
  return dec;
}
static void parseRMC(char *l) {
  if (!nmeaOk(l)) return;
  if (strncmp(l, "$GPRMC", 6) && strncmp(l, "$GNRMC", 6)) return;
  char *tok = strtok(l, ",");
  int f = 0;
  char status = 'V', ns = 'N', ew = 'E';
  float knots = 0;
  char timebuf[16] = {0}, datebuf[12] = {0}, latbuf[16] = {0}, lonbuf[16] = {0};
  while (tok) {
    if (f == 1 && tok[0]) strncpy(timebuf, tok, sizeof(timebuf)-1);
    else if (f == 2) status = tok[0];
    else if (f == 3 && tok[0]) strncpy(latbuf, tok, sizeof(latbuf)-1);
    else if (f == 4 && tok[0]) ns = tok[0];
    else if (f == 5 && tok[0]) strncpy(lonbuf, tok, sizeof(lonbuf)-1);
    else if (f == 6 && tok[0]) ew = tok[0];
    else if (f == 7) knots = atof(tok);
    else if (f == 9 && tok[0]) strncpy(datebuf, tok, sizeof(datebuf)-1);
    tok = strtok(NULL, ",");
    f++;
  }
  gpsFix = (status == 'A');
  if (gpsFix) {
    float mph = knots * 1.15078f;
    gpsMphSmooth += (mph - gpsMphSmooth) * 0.25f;
    gpsLat = nmeaToDec(latbuf, ns);
    gpsLon = nmeaToDec(lonbuf, ew);
    if (timebuf[0] && strlen(timebuf) >= 6) {
      snprintf(gpsTime, sizeof(gpsTime), "%c%c:%c%c:%c%c",
               timebuf[0], timebuf[1], timebuf[2], timebuf[3], timebuf[4], timebuf[5]);
    }
    if (datebuf[0] && strlen(datebuf) >= 6) {
      snprintf(gpsDate, sizeof(gpsDate), "%c%c/%c%c/%c%c",
               datebuf[0], datebuf[1], datebuf[2], datebuf[3], datebuf[4], datebuf[5]);
    }
    gpsLast = millis();
  }
}
static void gpsPump() {
  while (GPSSerial.available()) {
    char c = (char)GPSSerial.read();
    if (c == '\n') {
      if (gpsLen > 0) { gpsLine[gpsLen] = 0; parseRMC(gpsLine); }
      gpsLen = 0;
    } else if (c != '\r' && gpsLen < (int)sizeof(gpsLine) - 1) {
      gpsLine[gpsLen++] = c;
    }
  }
}
static void logGPS() {
  if (!sdOk || !gpsFix || millis() - lastLog < 5000) return;
  lastLog = millis();
  File f = SD_MMC.open("/topdrive_log.csv", FILE_APPEND);
  if (f) {
    f.printf("%s %s,%.6f,%.6f,%.1f\n", gpsDate, gpsTime, gpsLat, gpsLon, gpsMphSmooth);
    f.close();
  }
}

// ---- world state ----
static float roadOff = 0;
static float laneF = 1.0f;
static int laneT = 1;
static float pxs = 0;

struct Tree { float x, y; int r; };
static Tree trees[12];
struct TCar { int lane; float y, spd; uint16_t col; };
static TCar traf[5];

static int grassX(bool left) {
  int w = ROAD_X - 8;
  return left ? 4 + (rand() % w) : ROAD_X + ROAD_W + 4 + (rand() % w);
}

static void drawCar(int cx, int cy, uint16_t col) {
  int w = 24, h = 42;
  int x = cx - w / 2, y = cy - h / 2;
  uint16_t dark = rgb565(16, 16, 20);
  uint16_t glass = rgb565(120, 160, 185);
  fillRect(x - 2, y + 5, 4, 8, dark);
  fillRect(x + w - 2, y + 5, 4, 8, dark);
  fillRect(x - 2, y + h - 13, 4, 8, dark);
  fillRect(x + w - 2, y + h - 13, 4, 8, dark);
  fillRect(x, y, w, h, col);
  fillRect(x + 3, y + 6, w - 6, 6, glass);
  fillRect(x + 3, y + h - 11, w - 6, 4, rgb565(80, 100, 120));
  fillRect(x + 2, y - 1, 4, 3, rgb565(255, 240, 150));
  fillRect(x + w - 6, y - 1, 4, 3, rgb565(255, 240, 150));
  for (int i = 0; i < w; i++) { setPx(x + i, y, dark); setPx(x + i, y + h - 1, dark); }
  for (int i = 0; i < h; i++) { setPx(x, y + i, dark); setPx(x + w - 1, y + i, dark); }
}

// Honda Fit — tall hatch, long roof, near-vertical hatch glass. GBA-style shading + outlines.
static void drawFit(int cx, int cy) {
  int w = 28, h = 52;
  int x = cx - w / 2, y = cy - h / 2;
  uint16_t body = rgb565(240, 240, 235);
  uint16_t shade = rgb565(200, 200, 195);
  uint16_t glass = rgb565(110, 150, 180);
  uint16_t dark = rgb565(18, 18, 22);
  uint16_t rim = rgb565(180, 180, 185);
  // wheels + simple rim highlight
  fillRect(x - 3, y + 8, 5, 10, dark);
  fillRect(x + w - 2, y + 8, 5, 10, dark);
  fillRect(x - 3, y + h - 18, 5, 10, dark);
  fillRect(x + w - 2, y + h - 18, 5, 10, dark);
  fillRect(x - 2, y + 10, 2, 6, rim);
  fillRect(x + w - 1, y + 10, 2, 6, rim);
  fillRect(x - 2, y + h - 16, 2, 6, rim);
  fillRect(x + w - 1, y + h - 16, 2, 6, rim);
  // body + side shade
  fillRect(x, y, w, h, body);
  fillRect(x, y + 4, 3, h - 8, shade);
  fillRect(x + w - 3, y + 4, 3, h - 8, shade);
  // windshield
  fillRect(x + 5, y + 7, w - 10, 8, glass);
  // long roof
  fillRect(x + 4, y + 16, w - 8, 20, shade);
  // hatch glass
  fillRect(x + 6, y + 38, w - 12, 6, glass);
  // mirrors
  fillRect(x - 2, y + 12, 3, 4, dark);
  fillRect(x + w - 1, y + 12, 3, 4, dark);
  // lights
  fillRect(x + 3, y - 1, 5, 3, rgb565(255, 245, 160));
  fillRect(x + w - 8, y - 1, 5, 3, rgb565(255, 245, 160));
  fillRect(x + 3, y + h - 2, 5, 3, rgb565(190, 40, 40));
  fillRect(x + w - 8, y + h - 2, 5, 3, rgb565(190, 40, 40));
  // outline
  for (int i = 0; i < w; i++) { setPx(x + i, y, dark); setPx(x + i, y + h - 1, dark); }
  for (int i = 0; i < h; i++) { setPx(x, y + i, dark); setPx(x + w - 1, y + i, dark); }
}

// Class C RV — cab-over bunk, dual rears, side windows, stripe. Longer silhouette.
static void drawRv(int cx, int cy) {
  int w = 32, h = 80;
  int x = cx - w / 2, y = cy - h / 2;
  uint16_t body = rgb565(235, 235, 230);
  uint16_t shade = rgb565(190, 190, 185);
  uint16_t glass = rgb565(110, 150, 180);
  uint16_t dark = rgb565(18, 18, 22);
  uint16_t stripe = rgb565(170, 110, 50);
  // front axle + rear duals
  fillRect(x - 3, y + 12, 5, 10, dark);
  fillRect(x + w - 2, y + 12, 5, 10, dark);
  fillRect(x - 4, y + h - 24, 6, 14, dark);
  fillRect(x + w - 2, y + h - 24, 6, 14, dark);
  // body
  fillRect(x, y, w, h, body);
  // cab-over bunk (wider band)
  fillRect(x - 2, y + 16, w + 4, 16, body);
  fillRect(x + 2, y + 18, w - 4, 10, glass);
  // cab windshield
  fillRect(x + 5, y + 5, w - 10, 7, glass);
  // stripe
  fillRect(x + 2, y + 36, 4, h - 50, stripe);
  fillRect(x + w - 6, y + 36, 4, h - 50, stripe);
  // side windows
  fillRect(x + 3, y + 42, 5, 7, glass);
  fillRect(x + w - 8, y + 42, 5, 7, glass);
  fillRect(x + 3, y + 54, 5, 7, glass);
  fillRect(x + w - 8, y + 54, 5, 7, glass);
  // rear window
  fillRect(x + 8, y + h - 14, w - 16, 6, glass);
  // lights
  fillRect(x + 4, y - 1, 5, 3, rgb565(255, 245, 160));
  fillRect(x + w - 9, y - 1, 5, 3, rgb565(255, 245, 160));
  fillRect(x + 4, y + h - 2, 5, 3, rgb565(190, 40, 40));
  fillRect(x + w - 9, y + h - 2, 5, 3, rgb565(190, 40, 40));
  // outline
  for (int i = 0; i < w; i++) { setPx(x + i, y, dark); setPx(x + i, y + h - 1, dark); }
  for (int i = 0; i < h; i++) { setPx(x, y + i, dark); setPx(x + w - 1, y + i, dark); }
}

// ---- radios -> roadside items ----
// Every newly-seen BLE device spawns a roadside item: named devices become
// billboards carrying the device name, unnamed ones become signal pylons.
// Panel color is heat-mapped by RSSI. Items scroll with the world and retire
// at the bottom — the roadside mirrors the real RF environment.
#define RITEM_N 10
struct RItem { bool on; float x, y; uint8_t kind; char name[8]; uint16_t col; };
static RItem ritems[RITEM_N];
static char seenMac[24][18];
static int seenN = 0;

static uint16_t rssiCol(int rssi) {
  if (rssi >= -55) return rgb565(230, 60, 60);
  if (rssi >= -70) return rgb565(230, 200, 60);
  return rgb565(60, 200, 90);
}

static void radioSpawn(const char *mac, const char *name, int rssi) {
  for (int i = 0; i < seenN; i++)
    if (strcmp(seenMac[i], mac) == 0) return;  // already on the roadside
  if (seenN < 24) { strncpy(seenMac[seenN], mac, 18); seenN++; }
  int slot = -1;
  for (int i = 0; i < RITEM_N; i++) if (!ritems[i].on) { slot = i; break; }
  if (slot < 0) return;
  RItem *it = &ritems[slot];
  it->on = true;
  bool left = rand() % 2;
  int gw = ROAD_X - 8;  // grass width each side
  it->x = left ? 4 : ROAD_X + ROAD_W + 4;
  it->y = -44;
  it->kind = (name && name[0]) ? 0 : 1;
  if (it->kind == 0) { strncpy(it->name, name, 7); it->name[7] = '\0'; }
  else it->name[0] = '\0';
  it->col = rssiCol(rssi);
}

static void drawRItems(uint32_t now) {
  for (int i = 0; i < RITEM_N; i++) {
    RItem *it = &ritems[i];
    if (!it->on) continue;
    int x = (int)it->x, y = (int)it->y;
    if (y < -50 || y > SCR_H + 50) continue;
    if (it->kind == 0) {
      // billboard: post + panel, device name on the sign
      fillRect(x + 21, y, 4, 28, rgb565(90, 90, 95));
      fillRect(x, y - 26, 46, 24, rgb565(20, 20, 28));
      fillRect(x, y - 26, 46, 2, it->col);
      fillRect(x, y - 4, 46, 2, it->col);
      drawText(x + 4, y - 21, it->name, 0xFFFF, 1);
    } else {
      // pylon: mast + crossarm + blinking tip
      fillRect(x + 20, y - 38, 5, 38, rgb565(110, 110, 115));
      fillRect(x + 12, y - 34, 21, 3, rgb565(110, 110, 115));
      uint16_t tip = (now / 500) % 2 ? rgb565(255, 60, 60) : rgb565(120, 20, 20);
      fillRect(x + 19, y - 44, 7, 5, tip);
    }
  }
}

// BLE discovery ring (callback runs on the BLE task; loop() drains it)
struct RBle { char mac[18]; char name[8]; int8_t rssi; };
static RBle rbleRing[16];
static std::atomic<int> rbleW{0}, rbleR{0};

class RScanCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) {
    int w = rbleW.load(std::memory_order_relaxed);
    int nx = (w + 1) % 16;
    if (nx == rbleR.load(std::memory_order_acquire)) return;
    String ms = d.getAddress().toString();
    ms.toCharArray(rbleRing[w].mac, sizeof(rbleRing[w].mac));
    rbleRing[w].name[0] = '\0';
    if (d.haveName()) {
      String nm = d.getName();
      nm.toCharArray(rbleRing[w].name, sizeof(rbleRing[w].name));
    }
    int r = d.getRSSI();
    if (r < -128) r = -128; if (r > 127) r = 127;
    rbleRing[w].rssi = (int8_t)r;
    rbleW.store(nx, std::memory_order_release);
  }
};

static void radioBegin() {
  BLEDevice::init("hh-drive");
  BLEScan *s = BLEDevice::getScan();
  s->setAdvertisedDeviceCallbacks(new RScanCB(), true);
  s->setActiveScan(true);
  s->setInterval(160);
  s->setWindow(120);
}

static void radioPoll(uint32_t now) {
  int r = rbleR.load(std::memory_order_relaxed);
  int w = rbleW.load(std::memory_order_acquire);
  while (r != w) {
    radioSpawn(rbleRing[r].mac, rbleRing[r].name, rbleRing[r].rssi);
    r = (r + 1) % 16;
  }
  rbleR.store(r, std::memory_order_release);
  BLEScan *s = BLEDevice::getScan();
  if (!s->isScanning()) { s->clearResults(); s->start(4, nullptr, false); }
}

static void render(float dt) {
  uint16_t grass = rgb565(22, 68, 26);
  uint16_t roadC = rgb565(46, 46, 50);
  fillRect(0, 0, SCR_W, SCR_H, grass);
  fillRect(ROAD_X, 0, ROAD_W, SCR_H, roadC);
  fillRect(ROAD_X + 3, 0, 3, SCR_H, 0xFFFF);
  fillRect(ROAD_X + ROAD_W - 6, 0, 3, SCR_H, 0xFFFF);
  for (int d = 0; d < LANES - 1; d++) {
    int dx = ROAD_X + LANE_W * (d + 1);
    int period = 56;
    int off = ((int)roadOff % period + period) % period;
    for (int y = -period; y < SCR_H + period; y += period)
      fillRect(dx - 2, y + off, 4, 26, rgb565(220, 220, 220));
  }
  for (int i = 0; i < 12; i++) {
    int ty = (int)trees[i].y;
    if (ty > -24 && ty < SCR_H + 24) {
      fillCircle((int)trees[i].x, ty, trees[i].r, rgb565(28, 105, 38));
      fillCircle((int)trees[i].x, ty, trees[i].r / 2, rgb565(55, 145, 65));
    }
  }
  int pcx = ROAD_X + (int)(LANE_W * laneF) + LANE_W / 2;
  bool hit = false;
  for (int i = 0; i < 5; i++) {
    int cx = ROAD_X + LANE_W * traf[i].lane + LANE_W / 2;
    int cy = (int)traf[i].y;
    if (cy > -60 && cy < SCR_H + 60) drawCar(cx, cy, traf[i].col);
    if (abs(cx - pcx) < vehHalfW() && abs(cy - CAR_Y) < vehHalfH()) hit = true;
  }
  drawRItems(millis());
  if (vehicle == 1) drawRv(pcx, CAR_Y);
  else drawFit(pcx, CAR_Y);
  if (hit) {
    int hw = vehHalfW(), hh = vehHalfH();
    fillRect(pcx - hw, CAR_Y - hh, hw * 2, hh * 2, rgb565(180, 30, 30));
  }

  // HUD
  char buf[16];
  bool haveGps = gpsFix && millis() - gpsLast < 5000;
  if (haveGps) snprintf(buf, sizeof(buf), "%d", (int)(gpsMphSmooth + 0.5f));
  else snprintf(buf, sizeof(buf), "--");
  drawText(12, 12, buf, 0xFFFF, 4);
  drawText(12, 44, "MPH", rgb565(160, 160, 160), 2);
  uint16_t dot = haveGps ? (gpsMphSmooth > 1.0f ? 0x07E0 : 0xFDA0) : 0xF800;
  fillRect(SCR_W - 18, 14, 8, 8, dot);
  char ln[8];
  snprintf(ln, sizeof(ln), "L%d", laneT + 1);
  drawText(SCR_W - 48, 36, ln, rgb565(200, 200, 200), 2);
  drawText(SCR_W - 48, 58, VEH_NAMES[vehicle], rgb565(160, 160, 160), 2);
  if (sdOk && haveGps) drawText(12, SCR_H - 28, "REC", rgb565(255, 60, 60), 2);
}

// Boot splash: the poster, banded pushes (320x48x2 = 30.7KB each, 4-aligned)
static void drawSplash() {
  const int SW = 320, SH = 476, ROWS = 48;
  uint8_t *band = (uint8_t *)heap_caps_malloc(SW * ROWS * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!band) return;
  for (int y = 0; y < SH; y += ROWS) {
    int h = SH - y; if (h > ROWS) h = ROWS;
    for (int i = 0; i < SW * h; i++) {
      uint16_t pxv = pgm_read_word(&SPLASH_IMG[y * SW + i]);
      band[i * 2] = pxv >> 8; band[i * 2 + 1] = pxv & 0xFF;
    }
    lcd->drawBitmap(0, 2 + y, SW, h, band);
  }
  heap_caps_free(band);
}

static void pushFrame() {
  const int ROWS = 50;
  for (int y = 0; y < SCR_H; y += ROWS) {
    int h = SCR_H - y;
    if (h > ROWS) h = ROWS;
    lcd->drawBitmap(0, y, SCR_W, h, &fb[(y * SCR_W) * 2]);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  pinMode(48, OUTPUT);
  digitalWrite(48, LOW); delay(120);
  digitalWrite(48, HIGH); delay(200);

  bus->configQSPI_FreqHz(80 * 1000 * 1000);
  lcd->configVendorCommands(vendor_init, sizeof(vendor_init) / sizeof(vendor_init[0]));
  if (!lcd->begin()) Serial.println("panel dark");
  lcd->displayOn();
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  drawSplash();
  delay(2500);

  fb = (uint8_t *)ps_malloc(SCR_W * SCR_H * 2);
  if (!fb) {
    Serial.println("Hardcore Henry: Drive — no PSRAM framebuffer - enable OPI PSRAM and reflash");
    while (1) delay(1000);
  }

  GPSSerial.begin(9600, SERIAL_8N1, 45, 46);
  radioBegin();  // BLE scan feeds the roadside items

  // SD 4-bit
  if (SD_MMC.setPins(5, 4, 6, 7, 2, 3)) {
    if (SD_MMC.begin("/sdcard", false)) {
      sdOk = true;
      if (!SD_MMC.exists("/topdrive_log.csv")) {
        File f = SD_MMC.open("/topdrive_log.csv", FILE_WRITE);
        if (f) {
          f.println("date time,lat,lon,speed_mph");
          f.close();
        }
      }
      Serial.println("Hardcore Henry: Drive — SD ok, logging to /topdrive_log.csv");
    } else {
      Serial.println("SD mount failed");
    }
  } else {
    Serial.println("SD pin set failed");
  }

  srand(millis());
  for (int i = 0; i < 12; i++) {
    trees[i].x = grassX(i % 2);
    trees[i].y = rand() % SCR_H;
    trees[i].r = 7 + rand() % 9;
  }
  uint16_t tcol[5] = { rgb565(60,120,220), rgb565(80,200,120), rgb565(220,200,60),
                       rgb565(200,200,200), rgb565(150,90,220) };
  for (int i = 0; i < 5; i++) {
    traf[i].lane = rand() % LANES;
    traf[i].y = rand() % SCR_H;
    traf[i].spd = 40 + rand() % 110;
    traf[i].col = tcol[i];
  }
}

void loop() {
  static uint32_t lastT = 0, lastBtn = 0, pressStart = 0;
  static bool prevBtn = false;
  uint32_t now = millis();
  float dt = (now - lastT) / 1000.0f;
  if (dt > 0.1f) dt = 0.1f;
  if (lastT == 0) dt = 0.016f;
  lastT = now;

  gpsPump();
  logGPS();
  radioPoll(now);
  for (int i = 0; i < RITEM_N; i++) if (ritems[i].on) {
    ritems[i].y += pxs * dt;
    if (ritems[i].y > SCR_H + 50) ritems[i].on = false;
  }

  bool haveGps = gpsFix && now - gpsLast < 5000;
  float target = haveGps ? gpsMphSmooth * 4.8f : 55.0f;
  if (target > 350.0f) target = 350.0f;
  pxs += (target - pxs) * (dt < 1 ? dt * 2.0f : 1.0f);

  bool btn = digitalRead(PIN_BOOT) == LOW;
  if (btn && !prevBtn) {
    pressStart = now;
  } else if (!btn && prevBtn) {
    if (now - pressStart >= 600) vehicle = (vehicle + 1) % 2;
    else if (now - lastBtn > 400) { lastBtn = now; laneT = (laneT + 1) % LANES; }
  }
  prevBtn = btn;
  laneF += (laneT - laneF) * (dt < 1 ? dt * 6.0f : 1.0f);

  roadOff += pxs * dt;

  for (int i = 0; i < 12; i++) {
    trees[i].y += pxs * dt;
    if (trees[i].y > SCR_H + 24) {
      trees[i].y = -24;
      trees[i].x = grassX(rand() % 2);
      trees[i].r = 7 + rand() % 9;
    }
  }
  bool nearHit = false;
  int pcx2 = ROAD_X + (int)(LANE_W * laneF) + LANE_W / 2;
  for (int i = 0; i < 5; i++) {
    traf[i].y += (pxs - traf[i].spd) * dt;
    if (abs((int)(ROAD_X + LANE_W * traf[i].lane + LANE_W / 2) - pcx2) < vehHalfW() &&
        abs((int)traf[i].y - CAR_Y) < vehHalfH()) nearHit = true;
    if (traf[i].y > SCR_H + 70) {
      traf[i].y = -70;
      traf[i].lane = rand() % LANES;
      traf[i].spd = 40 + rand() % 110;
    } else if (traf[i].y < -70) {
      traf[i].y = SCR_H + 70;
      traf[i].lane = rand() % LANES;
      traf[i].spd = 40 + rand() % 110;
    }
  }

  if (nearHit) pxs *= 0.6f;
  render(dt);
  pushFrame();
}
