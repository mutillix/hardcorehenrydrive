// HARDCORE HENRY DRIVE — WANDER MODE
// GTA1-style top-down autonomous wander for the Elecrow ES3C35P (ESP32-S3).
// The car drives itself through an endless procedural city; you ride along.
// Built as a back-seat / rideshare console gimmick: real BLE + WiFi radios
// around the car spawn NPC traffic, so the world mirrors the street.
// TAP BOOT = next vehicle (FIT / RV / PICKUP). HOLD BOOT = pause/resume.
// Speed is generated: 0-10mph = parked (brake lights on), 25mph+ = full flow.
// No GPS, no SD — pure simulation.
// Mike's pass merged: building windows, intersection-clean dashes, car shadows,
// headlight glow (with fixes: world-coord roof hash, true intersection skip zone).
// IDE: ESP32S3 Dev Module, Flash Size 8MB, PSRAM OPI PSRAM,
//   Partition Scheme "8M with spiffs (3MB APP/1.5MB SPIFFS)".
// ============================================================

#include <Arduino.h>
#include <esp_display_panel.hpp>
using namespace esp_panel::drivers;
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <atomic>
#include <Preferences.h>
#include "topdrive_splash.h"
#include "topdrive_world.h"

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

// ---- world ----
#define BLOCK 320
#define ROADW 90
#define PX_PER_MPH 6.25f

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
  {'A', 0x7E,0x11,0x11,0x11,0x7E}, {'U', 0x7F,0x40,0x40,0x40,0x7F},
  {'O', 0x3E,0x41,0x41,0x41,0x3E}, {'D', 0x7F,0x41,0x41,0x41,0x3E},
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

// ---- sprite buffers (cars are drawn facing up, then rotated) ----
#define SPR_S 128
#define NPC_S 64
#define CKEY 0xF81F
static uint8_t sprBuf[SPR_S * SPR_S * 2];
static uint8_t npcBuf[NPC_S * NPC_S * 2];

static inline void bpx(uint8_t *b, int W, int x, int y, uint16_t c) {
  if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)W) return;
  uint8_t *p = &b[(y * W + x) * 2];
  p[0] = c >> 8; p[1] = c & 0xFF;
}
static void brect(uint8_t *b, int W, int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > W) h = W - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++) {
    uint8_t *p = &b[((y + j) * W + x) * 2];
    for (int i = 0; i < w; i++) { p[0] = c >> 8; p[1] = c & 0xFF; p += 2; }
  }
}
static void bclear(uint8_t *b, int W) {
  for (int i = 0; i < W * W; i++) { b[i * 2] = CKEY >> 8; b[i * 2 + 1] = CKEY & 0xFF; }
}

// Honda Fit — tall hatch, long roof, hatch glass. GBA-style shading.
static void drawFitBuf(uint8_t *b) {
  int S = SPR_S, w = 35, h = 65, x = S/2 - w/2, y = S/2 - h/2;
  uint16_t body = rgb565(240, 240, 235);
  uint16_t shade = rgb565(200, 200, 195);
  uint16_t glass = rgb565(110, 150, 180);
  uint16_t dark = rgb565(18, 18, 22);
  uint16_t rim = rgb565(180, 180, 185);
  brect(b,S,x-4,y+10,6,13,dark); brect(b,S,x+w-3,y+10,6,13,dark);
  brect(b,S,x-4,y+h-23,6,13,dark); brect(b,S,x+w-3,y+h-23,6,13,dark);
  brect(b,S,x-3,y+13,3,8,rim); brect(b,S,x+w-1,y+13,3,8,rim);
  brect(b,S,x-3,y+h-20,3,8,rim); brect(b,S,x+w-1,y+h-20,3,8,rim);
  brect(b,S,x,y,w,h,body);
  brect(b,S,x,y+5,4,h-10,shade); brect(b,S,x+w-4,y+5,4,h-10,shade);
  brect(b,S,x+6,y+9,w-13,10,glass);
  brect(b,S,x+5,y+20,w-10,25,shade);
  brect(b,S,x+8,y+48,w-15,8,glass);
  brect(b,S,x-3,y+15,4,5,dark); brect(b,S,x+w-1,y+15,4,5,dark);
  brect(b,S,x+4,y-1,6,4,rgb565(255,245,160)); brect(b,S,x+w-10,y-1,6,4,rgb565(255,245,160));
  brect(b,S,x+4,y+h-3,6,4,rgb565(190,40,40)); brect(b,S,x+w-10,y+h-3,6,4,rgb565(190,40,40));
  for (int i = 0; i < w; i++) { bpx(b,S,x+i,y,dark); bpx(b,S,x+i,y+h-1,dark); }
  for (int i = 0; i < h; i++) { bpx(b,S,x,y+i,dark); bpx(b,S,x+w-1,y+i,dark); }
}

// Class C RV — cab-over bunk, dual rears, side windows, stripe.
static void drawRvBuf(uint8_t *b) {
  int S = SPR_S, w = 40, h = 100, x = S/2 - w/2, y = S/2 - h/2;
  uint16_t body = rgb565(235, 235, 230);
  uint16_t glass = rgb565(110, 150, 180);
  uint16_t dark = rgb565(18, 18, 22);
  uint16_t stripe = rgb565(170, 110, 50);
  brect(b,S,x-4,y+15,6,13,dark); brect(b,S,x+w-3,y+15,6,13,dark);
  brect(b,S,x-5,y+h-30,8,18,dark); brect(b,S,x+w-3,y+h-30,8,18,dark);
  brect(b,S,x,y,w,h,body);
  brect(b,S,x-3,y+20,w+5,20,body);
  brect(b,S,x+3,y+23,w-5,13,glass);
  brect(b,S,x+6,y+6,w-13,9,glass);
  brect(b,S,x+3,y+45,5,h-63,stripe); brect(b,S,x+w-8,y+45,5,h-63,stripe);
  brect(b,S,x+4,y+53,6,9,glass); brect(b,S,x+w-10,y+53,6,9,glass);
  brect(b,S,x+4,y+68,6,9,glass); brect(b,S,x+w-10,y+68,6,9,glass);
  brect(b,S,x+10,y+h-18,w-20,8,glass);
  brect(b,S,x+5,y-1,6,4,rgb565(255,245,160)); brect(b,S,x+w-11,y-1,6,4,rgb565(255,245,160));
  brect(b,S,x+5,y+h-3,6,4,rgb565(190,40,40)); brect(b,S,x+w-11,y+h-3,6,4,rgb565(190,40,40));
  for (int i = 0; i < w; i++) { bpx(b,S,x+i,y,dark); bpx(b,S,x+i,y+h-1,dark); }
  for (int i = 0; i < h; i++) { bpx(b,S,x,y+i,dark); bpx(b,S,x+w-1,y+i,dark); }
}

// Pickup truck — hood, cab, open bed.
static void drawPickupBuf(uint8_t *b) {
  int S = SPR_S, w = 35, h = 70, x = S/2 - w/2, y = S/2 - h/2;
  uint16_t body = rgb565(40, 90, 160);
  uint16_t shade = rgb565(28, 62, 116);
  uint16_t glass = rgb565(110, 150, 180);
  uint16_t dark = rgb565(18, 18, 22);
  uint16_t bed = rgb565(20, 44, 88);
  brect(b,S,x-4,y+10,6,13,dark); brect(b,S,x+w-3,y+10,6,13,dark);
  brect(b,S,x-4,y+h-23,6,13,dark); brect(b,S,x+w-3,y+h-23,6,13,dark);
  brect(b,S,x,y,w,h,body);
  brect(b,S,x,y+5,4,h-10,shade); brect(b,S,x+w-4,y+5,4,h-10,shade);
  brect(b,S,x+4,y+3,w-8,13,shade);            // hood
  brect(b,S,x+6,y+16,w-13,9,glass);           // windshield
  brect(b,S,x+5,y+26,w-10,11,body);             // cab roof
  brect(b,S,x+8,y+39,w-15,5,glass);           // cab rear window
  brect(b,S,x+4,y+45,w-8,h-50,bed);           // open bed
  brect(b,S,x+4,y+45,w-8,3,shade);            // bed front wall
  for (int i = 0; i < w-8; i += 4) bpx(b,S,x+4+i,y+h-5,shade);  // tailgate seam
  brect(b,S,x+4,y-1,6,4,rgb565(255,245,160)); brect(b,S,x+w-10,y-1,6,4,rgb565(255,245,160));
  brect(b,S,x+4,y+h-3,6,4,rgb565(190,40,40)); brect(b,S,x+w-10,y+h-3,6,4,rgb565(190,40,40));
  for (int i = 0; i < w; i++) { bpx(b,S,x+i,y,dark); bpx(b,S,x+i,y+h-1,dark); }
  for (int i = 0; i < h; i++) { bpx(b,S,x,y+i,dark); bpx(b,S,x+w-1,y+i,dark); }
}

static const char *VEH_NAMES[] = {"FIT", "RV", "PICKUP"};

// brake-light glow at the rear (car faces up in the buffer)
static void brakeGlow(uint8_t *b, int S, int w, int h) {
  int x = S/3 - w/3, y = S/3 - h/3;
  uint16_t brk = rgb565(255, 50, 50);
  brect(b,S,x+1,y+h-6,10,8,brk);
  brect(b,S,x+w-11,y+h-6,10,8,brk);
}

// generic NPC car
static void drawNpcBuf(uint8_t *b, uint16_t col) {
  int S = NPC_S, w = 30, h = 50, x = S/2 - w/2, y = S/2 - h/2;
  uint16_t dark = rgb565(16, 16, 20);
  uint16_t glass = rgb565(120, 160, 185);
  brect(b,S,x-3,y+6,5,10,dark); brect(b,S,x+w-3,y+6,5,10,dark);
  brect(b,S,x-3,y+h-16,5,10,dark); brect(b,S,x+w-3,y+h-16,5,10,dark);
  brect(b,S,x,y,w,h,col);
  brect(b,S,x+4,y+8,w-8,8,glass);
  brect(b,S,x+4,y+h-14,w-8,5,rgb565(80,100,120));
  brect(b,S,x+3,y-1,5,4,rgb565(255,240,150)); brect(b,S,x+w-8,y-1,5,4,rgb565(255,240,150));
  for (int i = 0; i < w; i++) { bpx(b,S,x+i,y,dark); bpx(b,S,x+i,y+h-1,dark); }
  for (int i = 0; i < h; i++) { bpx(b,S,x,y+i,dark); bpx(b,S,x+w-1,y+i,dark); }
}

// ---- pre-rendered 8-way sprites (no per-pixel trig at runtime) ----
#define DIRS 8
#define TAU 6.2831853f
static uint8_t *vehSpr[3][2];       // [vehicle][brake] -> DIRS frames of SxS
static int vehSprS[3] = {80, 112, 80};
static uint8_t *npcSpr[2];          // [ble/wifi] -> DIRS frames of 64x64
static uint8_t workBuf[128 * 128 * 2];

static int dirOf(float h) {
  float hn = fmodf(h, TAU);
  if (hn < 0) hn += TAU;
  return ((int)(hn / (TAU / DIRS) + 0.5f)) % DIRS;
}

// rotate src (SxS, faces up) by heading h into dst (both keyed)
static void renderDir(uint8_t *dst, uint8_t *src, int S, float h) {
  float ca = cosf(h), sa = sinf(h);
  int R = S / 2;
  uint8_t khi = CKEY >> 8, klo = CKEY & 0xFF;
  for (int y = 0; y < S; y++) {
    for (int x = 0; x < S; x++) {
      float dx = x - R, dy = y - R;
      float lx = dx * ca + dy * sa;
      float ly = -dx * sa + dy * ca;
      int ix = (int)(lx + R), iy = (int)(ly + R);
      int di = (y * S + x) * 2;
      if ((unsigned)ix >= (unsigned)S || (unsigned)iy >= (unsigned)S) {
        dst[di] = khi; dst[di + 1] = klo;
      } else {
        int si = (iy * S + ix) * 2;
        dst[di] = src[si]; dst[di + 1] = src[si + 1];
      }
    }
  }
}

static void buildVeh(int v) {
  int S = vehSprS[v];
  int w = v == 1 ? 40 : 35, h = v == 1 ? 100 : (v == 2 ? 70 : 65);
  bclear(sprBuf, SPR_S);
  if (v == 0) drawFitBuf(sprBuf);
  else if (v == 1) drawRvBuf(sprBuf);
  else drawPickupBuf(sprBuf);
  for (int b = 0; b < 2; b++) {
    if (b == 1) brakeGlow(sprBuf, SPR_S, w, h);  // brake variant = car + glow
    int off = (SPR_S - S) / 2;                   // crop center SxS
    for (int y = 0; y < S; y++)
      memcpy(&workBuf[y * S * 2], &sprBuf[((y + off) * SPR_S + off) * 2], S * 2);
    vehSpr[v][b] = (uint8_t *)ps_malloc(DIRS * S * S * 2);
    if (!vehSpr[v][b]) { Serial.println("veh sprite alloc fail"); return; }
    for (int d = 0; d < DIRS; d++)
      renderDir(&vehSpr[v][b][d * S * S * 2], workBuf, S, d * (TAU / DIRS));
  }
}

static void buildSprites() {
  for (int v = 0; v < 3; v++) buildVeh(v);
  for (int t = 0; t < 2; t++) {
    bclear(npcBuf, NPC_S);
    drawNpcBuf(npcBuf, t == 0 ? rgb565(120, 190, 230) : rgb565(230, 190, 110));
    npcSpr[t] = (uint8_t *)ps_malloc(DIRS * NPC_S * NPC_S * 2);
    if (!npcSpr[t]) { Serial.println("npc sprite alloc fail"); return; }
    for (int d = 0; d < DIRS; d++)
      renderDir(&npcSpr[t][d * NPC_S * NPC_S * 2], npcBuf, NPC_S, d * (TAU / DIRS));
  }
}

// fast masked copy, no trig
static void blitSpr(int cx, int cy, uint8_t *s, int S) {
  int R = S / 2;
  uint8_t khi = CKEY >> 8, klo = CKEY & 0xFF;
  for (int y = 0; y < S; y++) {
    int py = cy - R + y;
    if (py < 0 || py >= SCR_H) continue;
    for (int x = 0; x < S; x++) {
      int px = cx - R + x;
      if (px < 0 || px >= SCR_W) continue;
      int si = (y * S + x) * 2;
      if (s[si] == khi && s[si + 1] == klo) continue;
      uint8_t *p = &fb[(py * SCR_W + px) * 2];
      p[0] = s[si]; p[1] = s[si + 1];
    }
  }
}

// ---- procedural city ----

static uint32_t hash2(int x, int y) {
  uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663);
  h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
  return h;
}
// kind: 0 = buildings (rects filled), 1 = park, 2 = parking lot
static void genBlock(int bx, int by, int *kind, BRect *rects, int *nrect) {
  uint32_t h = hash2(bx, by);
  *kind = h % 3;
  *nrect = 0;
  int x0 = bx * BLOCK + ROADW, y0 = by * BLOCK + ROADW;
  if (*kind == 0) {
    for (int ly = 0; ly < 2; ly++) for (int lx = 0; lx < 2; lx++) {
      uint32_t h2 = hash2(bx * 4 + lx + 11, by * 4 + ly + 71);
      if ((h2 % 100) < 80 && *nrect < 4) {
        int bw = 58 + h2 % 42, bh = 58 + (h2 >> 7) % 42;
        rects[*nrect].x = x0 + lx * 115 + (115 - bw) / 2;
        rects[*nrect].y = y0 + ly * 115 + (115 - bh) / 2;
        rects[*nrect].w = bw; rects[*nrect].h = bh;
        (*nrect)++;
      }
    }
  }
}

static void renderWorld(float camX, float camY) {
  fillRect(0, 0, SCR_W, SCR_H, rgb565(24, 72, 30));
  int bx0 = (int)floorf(camX / BLOCK) - 1, bx1 = (int)floorf((camX + SCR_W) / BLOCK) + 1;
  int by0 = (int)floorf(camY / BLOCK) - 1, by1 = (int)floorf((camY + SCR_H) / BLOCK) + 1;
  for (int by = by0; by <= by1; by++) for (int bx = bx0; bx <= bx1; bx++) {
    int kind, nrect; BRect rects[4];
    genBlock(bx, by, &kind, rects, &nrect);
    int x0 = bx * BLOCK - (int)camX, y0 = by * BLOCK - (int)camY;
    if (kind == 2) {
      fillRect(x0 + ROADW, y0 + ROADW, BLOCK - ROADW, BLOCK - ROADW, rgb565(70, 70, 74));
      for (int s = 0; s < 6; s++)
        fillRect(x0 + ROADW + 18 + s * 35, y0 + ROADW + 25, 3, 50, rgb565(200, 200, 200));
    } else if (kind == 1) {
      for (int t = 0; t < 6; t++) {
        uint32_t h2 = hash2(bx * 9 + t, by * 9 + t * 2);
        int tx = x0 + ROADW + 15 + h2 % (BLOCK - ROADW - 30);
        int ty = y0 + ROADW + 15 + (h2 >> 9) % (BLOCK - ROADW - 30);
        fillCircle(tx, ty, 8 + (h2 >> 18) % 8, rgb565(30, 110, 42));
      }
    } else {
      for (int k = 0; k < nrect; k++) {
        int rx = rects[k].x - (int)camX, ry = rects[k].y - (int)camY;
        uint16_t roof = rgb565(60 + hash2(rects[k].x, rects[k].y) % 40,
                               55 + hash2(rects[k].y, rects[k].x) % 30, 50);
        fillRect(rx, ry, rects[k].w, rects[k].h, rgb565(52, 52, 58));
        fillRect(rx + 5, ry + 5, rects[k].w - 10, rects[k].h - 10, roof);
        for (int wy = 10; wy < rects[k].h - 10; wy += 15)
          for (int wx = 10; wx < rects[k].w - 10; wx += 15)
            fillRect(rx + wx, ry + wy, 6, 6, rgb565(180, 200, 220));
      }
    }
  }
  uint16_t asph = rgb565(46, 46, 50);
  for (int n = (int)floorf(camX / BLOCK) - 1; n <= (int)floorf((camX + SCR_W) / BLOCK) + 1; n++) {
    int sx = n * BLOCK - (int)camX;
    fillRect(sx, 0, ROADW, SCR_H, asph);
    fillRect(sx + 5, 0, 3, SCR_H, rgb565(200, 200, 200));
    fillRect(sx + ROADW - 8, 0, 3, SCR_H, rgb565(200, 200, 200));
    int wy0 = (int)floorf(camY / 80) * 80;
    for (int wy = wy0; wy < camY + SCR_H; wy += 80) {
      float rel = wy - floorf(wy / (float)BLOCK) * BLOCK;
      if (rel < ROADW + 10) continue;  // no dash inside intersections
      fillRect(sx + ROADW / 2 - 3, wy - (int)camY, 5, 38, rgb565(220, 200, 90));
    }
  }
  for (int n = (int)floorf(camY / BLOCK) - 1; n <= (int)floorf((camY + SCR_H) / BLOCK) + 1; n++) {
    int sy = n * BLOCK - (int)camY;
    fillRect(0, sy, SCR_W, ROADW, asph);
    fillRect(0, sy + 5, SCR_W, 3, rgb565(200, 200, 200));
    fillRect(0, sy + ROADW - 8, SCR_W, 3, rgb565(200, 200, 200));
    int wx0 = (int)floorf(camX / 80) * 80;
    for (int wx = wx0; wx < camX + SCR_W; wx += 80) {
      float rel = wx - floorf(wx / (float)BLOCK) * BLOCK;
      if (rel < ROADW + 10) continue;
      fillRect(wx - (int)camX, sy + ROADW / 2 - 3, 38, 5, rgb565(220, 200, 90));
    }
  }
}

static float wrapPi(float a) {
  while (a > 3.14159265f) a -= 6.2831853f;
  while (a < -3.14159265f) a += 6.2831853f;
  return a;
}

static void findRoads(float x, float y, float *bestV, float *cvx, float *bestH, float *chy) {
  int n0 = (int)floorf(x / BLOCK), m0 = (int)floorf(y / BLOCK);
  *bestV = 1e9;
  for (int n = n0 - 1; n <= n0 + 1; n++) {
    float cx = n * BLOCK + ROADW / 2;
    float d = fabsf(x - cx);
    if (d < *bestV) { *bestV = d; *cvx = cx; }
  }
  *bestH = 1e9;
  for (int m = m0 - 1; m <= m0 + 1; m++) {
    float cy = m * BLOCK + ROADW / 2;
    float d = fabsf(y - cy);
    if (d < *bestH) { *bestH = d; *chy = cy; }
  }
}

// Road following via pure pursuit: aim at a lookahead point on the road
// centerline. Hysteresis holds the current road through intersections
// (no more yank-turns); on entry, 22% chance of a deliberate left/right.
// st carries per-car state. Off-road: gentle seek toward the nearest road.
static float roadSteer(float x, float y, float head, SteerSt *st) {
  float bestV, cvx, bestH, chy;
  findRoads(x, y, &bestV, &cvx, &bestH, &chy);
  float dist = bestV < bestH ? bestV : bestH;
  if (dist > 175) {
    st->inInter = false; st->hasTarget = false;
    float tx = bestV < bestH ? cvx : x;
    float ty = bestV < bestH ? y : chy;
    return wrapPi(atan2f(tx - x, -(ty - y)) - head) * 0.7f;
  }

  bool inInter = (bestV < 75 && bestH < 75);
  if (inInter && !st->inInter) {
    // just entered: straight, or commit to a left/right turn
    st->hasTarget = false;
    if ((rand() % 100) < 22) {
      float turnA = wrapPi(head + ((rand() % 2) ? 1.5707963f : -1.5707963f));
      st->tx = cvx + sinf(turnA) * 163.0f;
      st->ty = chy - cosf(turnA) * 163.0f;
      st->hasTarget = true;
    }
  }
  if (!inInter) st->hasTarget = false;
  st->inInter = inInter;

  if (st->hasTarget) {
    float t = wrapPi(atan2f(st->tx - x, -(st->ty - y)) - head) * 2.5f;
    if (t > 2.5f) t = 2.5f; if (t < -2.5f) t = -2.5f;
    return t;
  }

  bool vert;
  if (bestV + 31 < bestH) vert = true;
  else if (bestH + 31 < bestV) vert = false;
  else vert = (fabsf(cosf(head)) >= fabsf(sinf(head)));  // ambiguous: trust heading

  const float L = 125.0f;  // lookahead distance, px (x1.25 with the world)
  float tx, ty;
  if (vert) {
    tx = cvx;
    ty = y + (cosf(head) > 0 ? -L : L);  // keep north/south sense
  } else {
    ty = chy;
    tx = x + (sinf(head) > 0 ? L : -L);  // keep east/west sense
  }
  float t = wrapPi(atan2f(tx - x, -(ty - y)) - head) * 2.5f;
  if (t > 2.5f) t = 2.5f; if (t < -2.5f) t = -2.5f;
  return t;
}

// circle-vs-buildings push-out
static void collidePt(float *x, float *y, float r) {
  int bx = (int)floorf(*x / BLOCK), by = (int)floorf(*y / BLOCK);
  for (int j = by - 1; j <= by + 1; j++) for (int i = bx - 1; i <= bx + 1; i++) {
    int kind, nrect; BRect rects[4];
    genBlock(i, j, &kind, rects, &nrect);
    if (kind != 0) continue;
    for (int k = 0; k < nrect; k++) {
      float rx = rects[k].x, ry = rects[k].y, rw = rects[k].w, rh = rects[k].h;
      float cx = *x < rx ? rx : (*x > rx + rw ? rx + rw : *x);
      float cy = *y < ry ? ry : (*y > ry + rh ? ry + rh : *y);
      float ddx = *x - cx, ddy = *y - cy;
      float d2 = ddx * ddx + ddy * ddy;
      if (d2 < r * r) {
        if (d2 > 0.01f) { float d = sqrtf(d2); *x = cx + ddx / d * r; *y = cy + ddy / d * r; }
        else { *x = rx - r; }
      }
    }
  }
}

// ---- player state ----
static float pwx = ROADW / 2, pwy = 200.0f;  // start mid-road
static float phead = 0.0f;                    // 0 = north
static float pspeed = 0.0f;                   // px/s
static float turnRate = 0.0f, wanderT = 0.0f;
static int vehicle = 0;                       // 0 FIT, 1 RV, 2 PICKUP
static int dstate = 0;                        // 0 parked, 1 cruise, 2 flow
static float stateT = 4.0f, targetMph = 0.0f;
static bool paused = false;
static SteerSt pSteer;

// trip odometer, persisted in NVS across resets
static Preferences prefs;
static float tripMiles = 0;
static uint32_t lastTripSave = 0;

static void driveTick(float dt) {
  stateT -= dt;
  if (stateT <= 0) {
    int r = rand() % 100;
    if (dstate == 0) { dstate = 1; targetMph = 12 + rand() % 11; stateT = 8 + rand() % 10; }
    else if (dstate == 1) {
      if (r < 60) { dstate = 2; targetMph = 26 + rand() % 13; stateT = 10 + rand() % 15; }
      else { dstate = 0; targetMph = 0; stateT = 3 + rand() % 4; }
    } else {
      if (r < 55) { dstate = 1; targetMph = 12 + rand() % 11; stateT = 8 + rand() % 12; }
      else if (r < 80) { dstate = 0; targetMph = 0; stateT = 3 + rand() % 4; }
      else { dstate = 2; targetMph = 26 + rand() % 13; stateT = 10 + rand() % 15; }
    }
  }
  float tsp = targetMph * PX_PER_MPH;
  float accel = (tsp > pspeed) ? 60.0f : 140.0f;
  if (pspeed < tsp) pspeed = tsp < pspeed + accel * dt ? tsp : pspeed + accel * dt;
  else pspeed = tsp > pspeed - accel * dt ? tsp : pspeed - accel * dt;

  wanderT += dt;
  float wwant = sinf(wanderT * 0.6f) * 0.5f + sinf(wanderT * 0.17f + 2.1f) * 0.35f;
  float want = wwant * 0.25f + roadSteer(pwx, pwy, phead, &pSteer);  // pursuit drives, wander whispers
  turnRate += (want - turnRate) * (dt < 1 ? dt * 3.0f : 1.0f);  // responsive, sim-tuned
  float spdF = pspeed / (30.0f * PX_PER_MPH); if (spdF > 1) spdF = 1;  // no turning while parked
  phead += turnRate * dt * spdF;
  pwx += sinf(phead) * pspeed * dt;
  pwy += -cosf(phead) * pspeed * dt;
  tripMiles += (pspeed / PX_PER_MPH) * dt / 3600.0f;  // mph * s / 3600 = miles
  collidePt(&pwx, &pwy, 18.0f);
  {
    static float sx = 0, sy = 0, st = 0;
    st += dt;
    if (st > 2.0f) {
      float dx = pwx - sx, dy = pwy - sy;
      if (pspeed > 50 && dx * dx + dy * dy < 625) turnRate += (rand() % 2 ? 2.5f : -2.5f);
      sx = pwx; sy = pwy; st = 0;
    }
  }
}

// ---- NPC traffic from radios ----
#define NPC_N 6
struct Npc { bool on; float x, y, head, speed, wt; uint32_t born; uint16_t col; uint8_t type; SteerSt steer; };
static Npc npcs[NPC_N];
static char seenMac[24][18];
static int seenN = 0, seenHead = 0;

static void npcSpawn(uint8_t type, int rssi) {
  int slot = -1;
  for (int i = 0; i < NPC_N; i++) if (!npcs[i].on) { slot = i; break; }
  if (slot < 0) return;
  Npc *n = &npcs[slot];
  n->on = true;
  float ah = phead + (rand() % 200 - 100) * 0.01f;
  float dist = 275 + rand() % 200;
  n->x = pwx + sinf(ah) * dist;
  n->y = pwy - cosf(ah) * dist;
  n->head = (rand() % 628) * 0.01f;
  float mph = 8 + (rssi + 95) * 0.45f;
  if (mph < 6) mph = 6; if (mph > 34) mph = 34;
  n->speed = mph * PX_PER_MPH;
  n->born = millis();
  n->type = type;
  n->wt = (rand() % 628) * 0.01f;
  n->col = (type == 0) ? rgb565(120, 190, 230) : rgb565(230, 190, 110);  // BLE cyan, WiFi amber
}

static bool radioSeen(const char *id) {
  for (int i = 0; i < seenN; i++) if (!strcmp(seenMac[i], id)) return true;
  strncpy(seenMac[seenHead], id, 18);  // ring: oldest forgotten, traffic keeps coming
  seenHead = (seenHead + 1) % 24;
  if (seenN < 24) seenN++;
  return false;
}

// BLE discovery ring (callback runs on the BLE task; loop() drains it)
struct RBle { char mac[18]; int8_t rssi; };
static RBle rbleRing[16];
static std::atomic<int> rbleW{0}, rbleR{0};

class RScanCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) {
    int w = rbleW.load(std::memory_order_relaxed);
    int nx = (w + 1) % 16;
    if (nx == rbleR.load(std::memory_order_acquire)) return;
    d.getAddress().toString().toCharArray(rbleRing[w].mac, sizeof(rbleRing[w].mac));
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

static void radioPoll() {
  int r = rbleR.load(std::memory_order_relaxed);
  int w = rbleW.load(std::memory_order_acquire);
  while (r != w) {
    if (!radioSeen(rbleRing[r].mac)) npcSpawn(0, rbleRing[r].rssi);
    r = (r + 1) % 16;
  }
  rbleR.store(r, std::memory_order_release);
  BLEScan *s = BLEDevice::getScan();
  if (!s->isScanning()) { s->clearResults(); s->start(4, nullptr, false); }
  int sc = WiFi.scanComplete();
  if (sc >= 0) {
    for (int i = 0; i < sc && i < 24; i++) {
      String bs = WiFi.BSSIDstr(i);
      char id[18]; bs.toCharArray(id, sizeof(id));
      if (!radioSeen(id)) npcSpawn(1, WiFi.RSSI(i));
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  } else if (sc == -2) {
    WiFi.scanNetworks(true);
  }
}

// GTA1-style minimap: roads + player + NPC dots
#define MM_S 76
#define MM_X (SCR_W - MM_S - 8)
#define MM_Y (SCR_H - MM_S - 8)
static void drawMinimap() {
  fillRect(MM_X - 2, MM_Y - 2, MM_S + 4, MM_S + 4, rgb565(10, 10, 14));
  for (int my = 0; my < MM_S; my++) {
    for (int mx = 0; mx < MM_S; mx++) {
      float wx = pwx + (mx - MM_S / 2) * 40.0f;
      float wy = pwy + (my - MM_S / 2) * 40.0f;
      float fx = wx - floorf(wx / BLOCK) * BLOCK;
      float fy = wy - floorf(wy / BLOCK) * BLOCK;
      setPx(MM_X + mx, MM_Y + my,
            (fx < ROADW || fy < ROADW) ? rgb565(90, 90, 96) : rgb565(16, 48, 20));
    }
  }
  for (int i = 0; i < NPC_N; i++) if (npcs[i].on) {
    int mx = MM_X + MM_S / 2 + (int)((npcs[i].x - pwx) / 40.0f);
    int my = MM_Y + MM_S / 2 + (int)((npcs[i].y - pwy) / 40.0f);
    if (mx >= MM_X && mx < MM_X + MM_S && my >= MM_Y && my < MM_Y + MM_S)
      fillRect(mx - 1, my - 1, 3, 3, npcs[i].col);
  }
  fillRect(MM_X + MM_S / 2 - 1, MM_Y + MM_S / 2 - 1, 3, 3, 0xFFFF);  // you
}

// ---- splash ----
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

// Push the framebuffer the same way the splash does: 48-row bands
// (4-aligned) through a temp buffer. Pushing straight from fb in 50-row
// bands left the poster on screen.
static uint8_t *pushBand = nullptr;
static void pushFrame() {
  const int ROWS = 48;
  if (!pushBand) {
    pushBand = (uint8_t *)heap_caps_malloc(SCR_W * ROWS * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pushBand) return;
  }
  for (int y = 0; y < SCR_H; y += ROWS) {
    int h = SCR_H - y;
    if (h > ROWS) h = ROWS;
    memcpy(pushBand, &fb[(y * SCR_W) * 2], SCR_W * h * 2);
    lcd->drawBitmap(0, y, SCR_W, h, pushBand);
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

  srand(millis());
  prefs.begin("hhdrive", false);
  tripMiles = prefs.getFloat("trip_mi", 0);
  buildSprites();  // pre-render 8-way cars (needs PSRAM framebuffer first)
  radioBegin();  // BLE scan feeds NPC traffic
  WiFi.mode(WIFI_STA);
  WiFi.scanNetworks(true);  // async; results picked up in loop()
}

void loop() {
  static uint32_t lastT = 0, lastBtn = 0, pressStart = 0;
  static bool prevBtn = false;
  uint32_t now = millis();
  float dt = (now - lastT) / 1000.0f;
  if (dt > 0.1f) dt = 0.1f;
  if (lastT == 0) dt = 0.016f;
  lastT = now;

  bool btn = digitalRead(PIN_BOOT) == LOW;
  if (btn && !prevBtn) {
    pressStart = now;
  } else if (!btn && prevBtn) {
    if (now - pressStart >= 600) paused = !paused;      // hold: pause
    else vehicle = (vehicle + 1) % 3;                   // tap: next vehicle
  }
  prevBtn = btn;

  if (!paused) {
    driveTick(dt);
    for (int i = 0; i < NPC_N; i++) if (npcs[i].on) {
      Npc *n = &npcs[i];
      n->wt += dt;
      float wwant = sinf(n->wt * 0.6f) * 0.5f + sinf(n->wt * 0.19f + 1.3f) * 0.35f;
      n->head += (wwant * 0.25f + roadSteer(n->x, n->y, n->head, &n->steer)) * dt * 0.9f;
      n->x += sinf(n->head) * n->speed * dt;
      n->y += -cosf(n->head) * n->speed * dt;
      collidePt(&n->x, &n->y, 15.0f);
      float dx = n->x - pwx, dy = n->y - pwy;
      if (dx * dx + dy * dy > 1000.0f * 1000.0f || now - n->born > 60000) n->on = false;
    }
  }
  radioPoll();
  if (now - lastTripSave > 60000) {  // persist the odometer every minute
    prefs.putFloat("trip_mi", tripMiles);
    lastTripSave = now;
  }

  float camX = pwx + sinf(phead) * 50 - SCR_W / 2;
  float camY = pwy - cosf(phead) * 50 - SCR_H / 2;
  renderWorld(camX, camY);

  int pcx = SCR_W / 2, pcy = SCR_H / 2;
  for (int i = 0; i < NPC_N; i++) if (npcs[i].on) {
    int sx = (int)(npcs[i].x - camX), sy = (int)(npcs[i].y - camY);
    if (sx < -40 || sx > SCR_W + 40 || sy < -40 || sy > SCR_H + 40) continue;
    int nd = dirOf(npcs[i].head);
    if (npcSpr[npcs[i].type]) {
      fillRect(sx - 13, sy + 18, 25, 8, rgb565(0, 0, 0));  // shadow
      blitSpr(sx, sy, &npcSpr[npcs[i].type][nd * NPC_S * NPC_S * 2], NPC_S);
    }
  }

  bool braking = (dstate == 0) || (pspeed > targetMph * PX_PER_MPH + 8);
  int S = vehSprS[vehicle];
  int d = dirOf(phead);
  if (vehSpr[vehicle][braking ? 1 : 0]) {
    fillRect(pcx - 15, pcy + 20, 30, 9, rgb565(0, 0, 0));  // shadow
    blitSpr(pcx, pcy, &vehSpr[vehicle][braking ? 1 : 0][d * S * S * 2], S);
  }
  if (pspeed > 5.0f)  // headlight glow ahead
    fillCircle(pcx + (int)(sinf(phead) * 35), pcy - (int)(cosf(phead) * 35),
               8, rgb565(255, 240, 160));

  // HUD
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", (int)(pspeed / PX_PER_MPH + 0.5f));
  drawText(12, 12, buf, 0xFFFF, 4);
  drawText(12, 44, "MPH", rgb565(160, 160, 160), 2);
  drawText(SCR_W - 96, 12, "AUTO", rgb565(90, 220, 120), 2);
  drawText(SCR_W - 96, 34, VEH_NAMES[vehicle], rgb565(200, 200, 200), 2);
  char tbuf[16];
  snprintf(tbuf, sizeof(tbuf), "%.1f MI", tripMiles);
  drawText(12, 66, tbuf, rgb565(160, 160, 160), 2);
  drawMinimap();
  if (paused) {
    drawText(SCR_W/2 - 42, SCR_H/2 - 40, "PAUSED", 0xFFFF, 2);
    drawText(SCR_W/2 - 90, SCR_H/2 - 18, "HOLD TO RESUME", rgb565(180, 180, 180), 2);
  }

  pushFrame();
}
