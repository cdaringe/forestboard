#pragma once
#include "Arduino.h"
#include <string>
constexpr int SH110X_WHITE = 1, SH110X_BLACK = 0;
constexpr uint8_t SH110X_DISPLAYON = 0xAF;
constexpr uint8_t SH110X_DISPLAYOFF = 0xAE;
inline std::vector<std::vector<uint8_t>> oledCommands;
inline bool failOled = false;
inline unsigned pageWrites = 0;
class Adafruit_SPIDevice {
public:
  bool write(const uint8_t*, size_t) {
    ++pageWrites;
    return !failOled;
  }
};
class Adafruit_SH1107 {
public:
  template <class... T> Adafruit_SH1107(T...) {}
  virtual ~Adafruit_SH1107() = default;
  void clearDisplay() {
    memset(pixels, 0, sizeof pixels);
    lastText.clear();
  }
  uint8_t* getBuffer() {
    return pixels;
  }
  int16_t width() const {
    return 128;
  }
  int16_t height() const {
    return 128;
  }
  bool oled_commandList(const uint8_t* data, uint8_t n) {
    oledCommands.emplace_back(data, data + n);
    return !failOled;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) {
    if (x < 0 || y < 0 || x >= 128 || y >= 128) {
      return;
    }
    uint8_t& p = pixels[x + 128 * (y / 8)];
    if (c) {
      p |= 1 << (y % 8);
    } else {
      p &= ~(1 << (y % 8));
    }
  }
  void drawLine(int16_t x, int16_t y, int16_t x1, int16_t y1, uint16_t c) {
    int dx = abs(x1 - x), sx = x < x1 ? 1 : -1, dy = -abs(y1 - y),
        sy = y < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
      drawPixel(x, y, c);
      if (x == x1 && y == y1) {
        break;
      }
      int e = 2 * err;
      if (e >= dy) {
        err += dy;
        x += sx;
      }
      if (e <= dx) {
        err += dx;
        y += sy;
      }
    }
  }
  void drawFastHLine(int x, int y, int w, int c) {
    if (w > 0) {
      drawLine(x, y, x + w - 1, y, c);
    }
  }
  void drawFastVLine(int x, int y, int h, int c) {
    if (h > 0) {
      drawLine(x, y, x, y + h - 1, c);
    }
  }
  unsigned rectCalls = 0, ellipseCalls = 0;
  void drawRect(int x, int y, int w, int h, int c) {
    ++rectCalls;
    drawFastHLine(x, y, w, c);
    drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c);
    drawFastVLine(x + w - 1, y, h, c);
  }
  void fillRect(int x, int y, int w, int h, int c) {
    for (int i = 0; i < h; ++i) {
      drawFastHLine(x, y + i, w, c);
    }
  }
  void drawTriangle(int x, int y, int a, int b, int u, int v, int c) {
    drawLine(x, y, a, b, c);
    drawLine(a, b, u, v, c);
    drawLine(u, v, x, y, c);
  }
  void drawEllipse(int x, int y, int rx, int ry, int c) {
    ++ellipseCalls;
    if (rx < 0 || ry < 0) {
      return;
    }
    for (int i = 0; i < 256; ++i) {
      const float a = i * 6.2831853f / 256;
      drawPixel(x + lroundf(rx * cosf(a)), y + lroundf(ry * sinf(a)), c);
    }
  }
  void drawCircle(int x, int y, int r, int c) {
    drawEllipse(x, y, r, r, c);
  }
  void fillCircle(int x, int y, int r, int c) {
    for (int a = -r; a <= r; ++a) {
      for (int b = -r; b <= r; ++b) {
        if (a * a + b * b <= r * r) {
          drawPixel(x + a, y + b, c);
        }
      }
    }
  }
  void drawRoundRect(int x, int y, int w, int h, int, int c) {
    drawRect(x, y, w, h, c);
  }
  void fillRoundRect(int x, int y, int w, int h, int, int c) {
    fillRect(x, y, w, h, c);
  }
  void setTextSize(int) {}
  void setTextColor(int) {}
  void setTextWrap(bool) {}
  int cursorX = 0, cursorY = 0;
  std::string lastText;
  void setCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
  }
  void print(const char* text) {
    lastText = text;
  }
  template <class T> void print(T) {}

protected:
  bool _init(uint8_t, bool) {
    return !failOled;
  }
  Adafruit_SPIDevice device;
  Adafruit_SPIDevice* spi_dev = &device;

private:
  uint8_t pixels[2048] = {};
};
