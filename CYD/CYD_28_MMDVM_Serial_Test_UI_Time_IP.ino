#include <SPI.h>
#include <TFT_eSPI.h>

// Standalone CYD ESP32-2432S028 D-Star/DMR dashboard test.
// TFT_eSPI uses the existing User_Setup.h selected by User_Setup_Select.h:
// ILI9341, MOSI=13, MISO=12, SCLK=14, CS=15, DC=2, RST=-1, BL=21 active HIGH.

static const uint32_t SERIAL_BAUD = 115200;
static const uint16_t SCREEN_WIDTH = 320;
static const size_t SERIAL_LINE_CAPACITY = 256;

TFT_eSPI tft;

String callsign = "N0CALL";
String modeName = "DMR";
String statusText = "WAITING";
String txText = "IDLE";
String rxText = "--";
String footerText = "Waiting for Pi-Star serial data";
String piTime = "--:--";
String piIp = "IP: ---";

char serialLine[SERIAL_LINE_CAPACITY];
size_t serialLength = 0;
bool serialOverflow = false;

String displayTxState(const String &value) {
  if (value == "IDLE") return "Stand by";
  return value;
}

void drawValueText(int x, int y, int w, int h,
                   const String &value, uint16_t color) {
  // Refresh only this field's value area to avoid full-screen flicker.
  tft.fillRect(x + 3, y + 20, w - 6, h - 23, TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(x + 8, y + 23);

  String shown = (x == 164 && y == 104) ? displayTxState(value) : value;
  const size_t maxChars = (size_t)((w - 16) / 12);
  if (shown.length() > maxChars) shown = shown.substring(0, maxChars);
  tft.print(shown);
}

void drawValueBox(int x, int y, int w, int h, const char *label,
                  const String &value, uint16_t color) {
  tft.drawRoundRect(x, y, w, h, 5, TFT_DARKGREY);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(x + 8, y + 7);
  tft.print(label);
  drawValueText(x, y, w, h, value, color);
}

void drawLastRx() {
  tft.fillRect(12, 181, 296, 22, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(16, 185);

  String shown = rxText;
  if (shown.length() > 24) shown = shown.substring(0, 24);
  tft.print(shown);
}

void drawFooter() {
  tft.fillRect(0, 212, SCREEN_WIDTH, 28, TFT_DARKGREEN);
  tft.setTextSize(1);

  // Left: IP address reported by Pi-Star. Right: device attribution.
  tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  tft.setCursor(5, 222);
  String shownIp = piIp;
  if (shownIp.length() > 18) shownIp = shownIp.substring(0, 18);
  tft.print(shownIp);

  const char *brand = "DEV MIOTECH By: HS8MIO";
  int brandWidth = tft.textWidth(brand);
  tft.setCursor(SCREEN_WIDTH - brandWidth - 5, 222);
  tft.print(brand);
}

void drawDashboard() {
  // Full frame is drawn only once during startup.
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, SCREEN_WIDTH, 32, TFT_NAVY);
  // MMDVM wordmark at top-left; clock is supplied by Pi-Star over Serial.
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.print("MMDVM");
  tft.setTextColor(TFT_YELLOW, TFT_NAVY);
  tft.setTextSize(2);
  int clockWidth = tft.textWidth(piTime);
  tft.setCursor(SCREEN_WIDTH - clockWidth - 8, 8);
  tft.print(piTime);

  drawValueBox(8, 42, 148, 54, "CALLSIGN", callsign, TFT_WHITE);
  drawValueBox(164, 42, 148, 54, "MODE", modeName, TFT_GREEN);
  drawValueBox(8, 104, 148, 54, "LINK STATUS", statusText,
               statusText == "CONNECTED" ? TFT_GREEN : TFT_YELLOW);
  drawValueBox(164, 104, 148, 54, "TX STATE", txText,
               txText == "TRANSMITTING" ? TFT_RED : TFT_WHITE);

  tft.drawRoundRect(8, 166, 304, 39, 5, TFT_DARKGREY);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(16, 173);
  tft.print("LAST RX");
  drawLastRx();
  drawFooter();
}

void redrawChangedFields(const String &oldCallsign, const String &oldMode,
                         const String &oldStatus, const String &oldTx,
                         const String &oldRx, const String &oldFooter) {
  if (callsign != oldCallsign)
    drawValueText(8, 42, 148, 54, callsign, TFT_WHITE);
  if (modeName != oldMode)
    drawValueText(164, 42, 148, 54, modeName, TFT_GREEN);
  if (statusText != oldStatus)
    drawValueText(8, 104, 148, 54, statusText,
                  statusText == "CONNECTED" ? TFT_GREEN : TFT_YELLOW);
  if (txText != oldTx)
    drawValueText(164, 104, 148, 54, txText,
                  txText == "TRANSMITTING" ? TFT_RED : TFT_WHITE);
  if (rxText != oldRx) drawLastRx();
  if (footerText != oldFooter) drawFooter();
}

void setField(const String &key, const String &value) {
  if (key == "CALL") callsign = value;
  else if (key == "MODE") modeName = value;
  else if (key == "STATUS") statusText = value;
  else if (key == "TX") txText = value;
  else if (key == "RX") rxText = value;
  else if (key == "TIME") piTime = value;
  else if (key == "IP") piIp = "IP: " + value;
}

void processLine(char *line) {
  String input(line);
  input.trim();
  if (input.length() == 0) return;

  const String oldCallsign = callsign;
  const String oldMode = modeName;
  const String oldStatus = statusText;
  const String oldTx = txText;
  const String oldRx = rxText;
  const String oldFooter = footerText;
  const String oldTime = piTime;
  const String oldIp = piIp;

  // Format: CALL=...;MODE=...;STATUS=...;TX=...;RX=...;TIME=HH:MM;IP=...\n
  int start = 0;
  while (start < input.length()) {
    int end = input.indexOf(';', start);
    if (end < 0) end = input.length();

    String field = input.substring(start, end);
    field.trim();
    int equals = field.indexOf('=');
    if (equals > 0) {
      String key = field.substring(0, equals);
      String value = field.substring(equals + 1);
      key.trim();
      value.trim();
      key.toUpperCase();
      setField(key, value);
    }
    start = end + 1;
  }
  footerText = "Pi-Star update received";
  redrawChangedFields(oldCallsign, oldMode, oldStatus, oldTx, oldRx, oldFooter);
  if (piTime != oldTime) {
    tft.fillRect(190, 0, 130, 32, TFT_NAVY);
    tft.setTextColor(TFT_YELLOW, TFT_NAVY);
    tft.setTextSize(2);
    int clockWidth = tft.textWidth(piTime);
    tft.setCursor(SCREEN_WIDTH - clockWidth - 8, 8);
    tft.print(piTime);
  }
  if (piIp != oldIp) drawFooter();
}

void pollSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      if (serialOverflow) {
        if (footerText != "Input line too long; discarded") {
          footerText = "Input line too long; discarded";
          drawFooter();
        }
      } else {
        serialLine[serialLength] = '\0';
        processLine(serialLine);
      }
      serialLength = 0;
      serialOverflow = false;
    } else if (!serialOverflow) {
      if (serialLength < SERIAL_LINE_CAPACITY - 1)
        serialLine[serialLength++] = c;
      else
        serialOverflow = true;
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(150);

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  tft.init();
  tft.setRotation(1);
  drawDashboard();

  Serial.println("CYD dashboard ready; send newline-delimited KEY=VALUE fields");
}

void loop() {
  pollSerial();
}
