// Screen size = 800 x 480
// CPU Type: ESP32S3 Dev Module
// PSRAM: OPI PSRAM Enabled
// Partitiion Scheme: Huge App
// Needs lvgl v8.3 or above
////////////////////////////////////////////////////////////////////////////////////
String version_num = "Elecrow ESP32S3 HA  v3";
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "credentials.h"  // Contains const char* ssid "YourSSID" and const char* password "YourPassword"
#include "symbols.h"      // Weather symbols
#include "gfx_conf.h"

#define degcode 247
#include <HTTPClient.h>
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson

const char* ntpServer = "uk.pool.ntp.org";
const char* Timezone = "GMT0BST,M3.5.0/01,M10.5.0/02";
const long gmtOffset_sec = 0;      // Set your offset in seconds from GMT e.g. if Europe then 3600
const int daylightOffset_sec = 0;  // Set your offset in seconds for Daylight saving

enum alignment { LEFT,
                 RIGHT,
                 CENTER };

enum display_mode { _temperature,
                    _humidity,
                    _dewpoint,
                    _windchill,
                    _battery };

String soc, cputemperature, temperature, pressure, humidity, dewpoint, uvindex, feelslike, probprecip, windspeed, winddirection, windgust, visibilitydistance, weather, metoweather;
String UpdateTime, UpdateDateTime, Icon, z_code, forecast;
int TimeStamp, solarradiation, z_month, message;
uint32_t previousMillis = 0;
int loopDelay = 15 * 60000;  // 15-mins
bool Refresh, drawn, trans_battery_flag;
float pTrend;
uint16_t touchX, touchY;
float reading[4]; // An array covering 24-hours to enable P, T, % and Wx state to be recorded for every hour
int reading_index = 0;
//#########################################################################################

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(__FILE__);
  lcd.begin();
  lcd.setRotation(0);      // 0-3
  lcd.setBrightness(128);  // 0-255
  lcd.setColorDepth(16);   // RGB565
  lcd.fillScreen(TFT_GREEN);
  lcd.setRotation(0);  // 0-3
  lcd.setTextFont(1);  // 1 is the default, 2 is OK but lacks extended chars. 3 is the same as 1, but 4 is OK
  clear_screen();      // Clear screen
  DisplayStatus(0, "Starting WiFi...");
  bool wifiStatus = StartWiFi();  // Start the WiFi service
  DisplayStatus(1, "Started WiFi...");
  DisplayStatus(2, "Starting Time Services...");
  StartTime();
  DisplayStatus(3, "Started Time Services...");
  DisplayStatus(4, "Getting Weather & Battery Data...");
  Refresh = true;  // To get fresh data
  GetEntityData("weather.met_office_melksham", &metoweather);
  pTrend = pressure.toFloat();
  reading[1] = pTrend; 
  reading[2] = pTrend; 
  reading[3] = pTrend; 
}

//#########################################################################################
void loop() {
  touchX = 0;
  touchY = 0;
  lcd.getTouch(&touchX, &touchY);
  if ((touchX >= 700) && (touchY <= 50)) {
    Refresh = true;
    DrawRefreshButton("Refreshing...");
  }
  if (millis() > previousMillis + loopDelay || Refresh == true) {
    GetTimeDate();
    GetHAEntities();
    clear_screen();
    previousMillis = millis();
    Refresh = false;
    drawString(150, 5, UpdateTime, CENTER, TFT_YELLOW, 2);
    drawString(300, 10, version_num, CENTER, TFT_YELLOW, 1);
    display_text(10, 48, String(temperature.toFloat(), 1) + char(degcode), TFT_GREEN, 3);  // char(247) is ° symbol or 96 is the newfont °
    display_text(140, 52, String(humidity.toFloat(), 0) + "%", TFT_GREEN, 2);              // char(247) is ° symbol
    int Dx = 10;
    int Dy = 80;
    display_text(Dx, Dy+=23, "Dew Point  = " + String(dewpoint.toFloat(),1) + char(degcode), TFT_CYAN, 2);
    display_text(Dx, Dy+=23, "Feels Like = " + String(feelslike.toFloat(),1) + char(degcode), TFT_CYAN, 2);
    display_text(Dx, Dy+=23, "Visibility = " + VisibilityCatergories(visibilitydistance), TFT_CYAN, 2);
    display_text(Dx, Dy+=23, "UV Index   = " + uvindex + uvindex_levels(uvindex), TFT_CYAN, 2);
    display_text(Dx, Dy+=23, "Wind-gust  = " + String(windgust.toFloat(), 1) + " mph", TFT_CYAN, 2);
    display_text(Dx, Dy+=23, "Pressure   = " + String(pressure.toFloat(), 1) + " hpa", TFT_CYAN, 2);
    lcd.drawRoundRect(0, 245, 500, 100, 12, TFT_YELLOW);
    pTrend = reading[0] - reading[3];
    reading_index += 1;
    if (reading_index > 3) reading_index = 0;
    Serial.println(pTrend);
    String PressureTrend_Str = "Steady";  // Either steady, climbing or falling
    if (pTrend > 0.05) PressureTrend_Str = "Rising";
    if (pTrend < -0.05) PressureTrend_Str = "Falling";
    z_code = calc_zambretti(pressure.toFloat(), z_month, winddirection.toFloat(), windspeed.toFloat(), pTrend);
    forecast = ZCode(z_code);
    display_text(10, 265, forecast, TFT_RED, 2);
    weather = formatWxStrings(weather);
    display_text(10, 300, "(" + toSentenceCase(weather) + ")", TFT_GREEN, 2);
    display_text(360, 223, " (" + PressureTrend_Str + " " + String(pTrend, 2) + "), " + z_code, TFT_CYAN, 1);

    DisplayWindDirection(410, 120, winddirection.toFloat(), windspeed.toFloat(), 75, TFT_YELLOW, TFT_RED, 2);
    gauge(50, 420, temperature.toFloat(), -10, 40, 20, 70, "Temperature", 0.8, _temperature);  // Low temperature starts at 20% when below 0° and high at 80% and above 28°
    gauge(175, 420, humidity.toFloat(), 0, 100, 40, 60, "Humidity", 0.8, _humidity);           // Low humidity at 40% and high at 60%
    gauge(300, 420, dewpoint.toFloat(), -10, 40, 20, 70, "Dew Point", 0.8, _temperature);      // Low temperature starts at 20% when below 0° and high at 80% and above 28°
    gauge(425, 420, feelslike.toFloat(), -10, 40, 20, 70, "Feelslike", 0.8, _windchill);       // Low temperature starts at 20% when below 0° and high at 80% and above 28°
    gauge(550, 420, soc.toFloat(), 0, 100, 20, 80, "Battery SoC", 0.8, _battery);              // Battery getting low at 20% and fully charged at 80%
    DrawRefreshButton("Refresh");
    if (WiFi.status() == WL_CONNECTED) display_text(5, 5, "Wi-Fi", (WL_CONNECTED ? TFT_CYAN: TFT_RED), 1);
    else StartWiFi();
  }
}
//#########################################################################################
void DrawRefreshButton(String txt) {
  lcd.setTextSize(2);
  if (txt == "Refresh") {
    lcd.setTextColor(TFT_BLACK, TFT_CYAN);
    lcd.fillRoundRect(700, 0, 100, 50, 12, TFT_CYAN);
    lcd.drawString(txt, 710, 15);
  }
  else {
    lcd.fillRoundRect(700 - txt.length() * 6, 0, 100 + txt.length() * 6, 50, 12, TFT_ORANGE);
    lcd.setTextColor(TFT_BLACK, TFT_ORANGE);
    lcd.drawString(txt, 710 - (txt.length() * 6), 15);
  }
  delay(2000);
}
//#########################################################################################
String toSentenceCase(String input) {
  if (input.length() == 0) return input; // Return if the string is empty
  input.toLowerCase();                   // Convert the entire string to lowercase
  input[0] = toupper(input[0]);          // Capitalize the first character
  return input;
}
//#########################################################################################
String formatWxStrings(String weather){
  if (weather == "partlycloudy") return "Partly cloudy";
  return weather;
}
//#########################################################################################
String uvindex_levels(String uvindex){
  int uvindex_level = uvindex.toInt();
  if (uvindex_level <= 2)                        return " (Low)";
  if (uvindex_level >= 3 && uvindex_level <= 5)  return " (Moderate)";
  if (uvindex_level >= 6 && uvindex_level <= 7)  return " (High)";
  if (uvindex_level >= 8 && uvindex_level <= 10) return " (Very High)";
  if (uvindex_level >= 11)                       return " (Extremely High)";
  return "";
}
//#########################################################################################
String VisibilityCatergories(String visibility){
  float visi = visibility.toFloat();
  if (visi < 1000)                    return "Very Poor";
  if (visi >= 1001  && visi <= 4000)  return "Poor";
  if (visi >= 4001  && visi <= 10000) return "Moderate";
  if (visi >= 10001 && visi <= 20000) return "Good";
  if (visi >= 20001 && visi <= 40000) return "Very Good";
  if (visi >  40000)                  return "Excellent";
  return visibility;
}
//#########################################################################################
void DisplayWindDirection(int x, int y, float angle, float windspeed, int Cradius, int color, int arrow_color, int Size) {
  int dxo, dyo, dxi, dyi;
  lcd.drawCircle(x, y, Cradius, color);         // Draw compass circle
  lcd.drawCircle(x, y, Cradius + 1, color);     // Draw compass circle
  lcd.drawCircle(x, y, Cradius * 0.75, color);  // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45) drawString(dxo + x + 15, dyo + y - 10, "NE", CENTER, color, Size);
    if (a == 135) drawString(dxo + x + 10, dyo + y + 5, "SE", CENTER, color, Size);
    if (a == 225) drawString(dxo + x - 15, dyo + y + 5, "SW", CENTER, color, Size);
    if (a == 315) drawString(dxo + x - 20, dyo + y - 10, "NW", CENTER, color, Size);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    lcd.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, color);
    dxo = dxo * 0.75;
    dyo = dyo * 0.75;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    lcd.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, color);
  }
  drawString(x - 0, y - Cradius - 18, "N", CENTER, color, Size);
  drawString(x - 0, y + Cradius + 5, "S", CENTER, color, Size);
  drawString(x - Cradius - 12, y - 8, "W", CENTER, color, Size);
  drawString(x + Cradius + 10, y - 8, "E", CENTER, color, Size);
  drawString(x - 2, y - 40, WindDegToOrdDirection(angle), CENTER, color, Size);
  drawString(x - 5, y - 15, String(windspeed, 1), CENTER, color, Size);
  drawString(x - 4, y + 5, "mph", CENTER, color, Size);
  drawString(x - 7, y + 30, String(angle, 0) + char(degcode), CENTER, color, Size);
  arrow(x, y, Cradius - 21, angle, 15, 25, arrow_color);  // Show wind direction on outer circle of width and length
}
//#########################################################################################
String WindDegToOrdDirection(float winddirection) {
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
  return Ord_direction[(dir % 16)];
}
//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength, int color) {
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x;  // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y;  // calculate Y position
  float x1 = 0;
  float y1 = plength;
  float x2 = pwidth / 2;
  float y2 = pwidth / 2;
  float x3 = -pwidth / 2;
  float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  lcd.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, color);
}
//#########################################################################################
float FtoC(float Value) {
  return (Value - 32) * 5.0 / 9.0;
}
//#########################################################################################
float InchesToHPA(float Value) {
  return Value * 33.863886666667;
}
//#########################################################################################
String ConvertUnixTime(int unix_time) {
  // http://www.cplusplus.com/reference/ctime/strftime/
  time_t tm = unix_time;
  struct tm* now_tm = gmtime(&tm);
  char output[40];
  strftime(output, sizeof(output), "%m", now_tm);
  z_month = String(output).toInt();
  strftime(output, sizeof(output), "%H:%M   %d/%m/%y", now_tm);
  return output;
}
//#########################################################################################
void clear_screen() {
  lcd.fillScreen(TFT_BLACK);
}
//#########################################################################################
void display_text(int x, int y, String text_string, int txt_colour, int txt_size) {
  lcd.setTextColor(txt_colour, TFT_BLACK);
  lcd.setTextSize(txt_size);
  lcd.setCursor(x, y);
  lcd.print(text_string);
}
//#########################################################################################
void drawString(int x, int y, String text_string, alignment align, int text_colour, int text_size) {
  int w = 2;  // The width of the font spacing
  lcd.setTextWrap(false);
  lcd.setTextColor(text_colour);
  lcd.setTextSize(text_size);
  if (text_size == 1) w = 4 * text_string.length();
  if (text_size == 2) w = 8 * text_string.length();
  if (text_size == 3) w = 12 * text_string.length();
  if (text_size == 4) w = 16 * text_string.length();
  if (text_size == 5) w = 20 * text_string.length();
  if (align == RIGHT) x = x - w;
  if (align == CENTER) x = x - (w / 2);
  lcd.setCursor(x, y);
  lcd.print(text_string);
  lcd.setTextSize(1);  // Back to default text size
}
//#########################################################################################
void DisplayStatus(int line, String message) {
  display_text(10, 20 + line * 25, message, TFT_GREEN, 2);
}
//#########################################################################################
bool StartWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);  // switch off AP
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected at: " + WiFi.localIP().toString());
  if (WiFi.status() != WL_CONNECTED) return false;
  else return true;
}
//#########################################################################################
void StartTime() {
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");  //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);                                                  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(100);
  GetTimeDate();
}
//#########################################################################################
void GetTimeDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 15000)) {
    Serial.println("Failed to obtain time");
  }
  char output[40];
  strftime(output, sizeof(output), "%m", &timeinfo);
  z_month = String(output).toInt();
  strftime(output, sizeof(output), "%H:%M %d/%m/%y", &timeinfo);
  UpdateDateTime = output;
  strftime(output, sizeof(output), "%H:%M", &timeinfo);
  UpdateTime = output;
}
//#########################################################################################
void DrawButton(int x, int y, int width, int height, int round, int buttonColour, int buttonTextColour, String buttonText) {
  lcd.fillRoundRect(x, y, width, height, round, buttonColour);
  lcd.setTextColor(buttonTextColour);
  lcd.setCursor(x + width / 2 - 10, y + height / 2 - 10);
  lcd.println(buttonText);
}
//#########################################################################################
int CorrectForWind(int zpressure, String windDirection, float windSpeed) {
  if (windSpeed > 0) {
    if (windDirection == "WNW") return zpressure - 0.5;
    if (windDirection == "E") return zpressure - 0.5;
    if (windDirection == "ESE") return zpressure - 2;
    if (windDirection == "W") return zpressure - 3;
    if (windDirection == "WSW") return zpressure - 4.5;
    if (windDirection == "SE") return zpressure - 5;
    if (windDirection == "SW") return zpressure - 6;
    if (windDirection == "SSE") return zpressure - 8.5;
    if (windDirection == "SSW") return zpressure - 10;
    if (windDirection == "S") return zpressure - 12;
    if (windDirection == "NW") return zpressure + 1;
    if (windDirection == "ENE") return zpressure + 2;
    if (windDirection == "NNW") return zpressure + 3;
    if (windDirection == "NE") return zpressure + 5;
    if (windDirection == "NNE") return zpressure + 5;
    if (windDirection == "N") return zpressure + 6;
  }
  return zpressure;
}
//#########################################################################################
String OrdinalWindDir(int dir) {
  int val = int((dir / 22.5) + 0.5);
  String arr[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
  return arr[(val % 16)];
}
//#########################################################################################
//                      1050 1045 1040 1035 1030 1025 1020 1015 1010 1005 1000 995  990  985  980  975  970  965  960  955
String risingCode[20] = { "A", "A", "A", "A", "A", "B", "B", "B", "C", "F", "G", "I", "J", "L", "M", "Q", "Q", "T", "Y", "Y" };
String fallingCode[20] = { "A", "A", "B", "B", "B", "D", "H", "O", "R", "R", "U", "V", "X", "X", "X", "Z", "Z", "Z", "Z", "Z" };
String steadyCode[20] = { "A", "A", "A", "A", "B", "B", "B", "E", "K", "N", "P", "S", "W", "W", "W", "X", "X", "Z", "Z", "Z" };
//#########################################################################################
String PressureToCode(int pressure, String trend) {
  if (pressure >= 1045) {
    if (trend == "Rising") return risingCode[0];
    if (trend == "Falling") return fallingCode[0];
    if (trend == "Steady") return steadyCode[0];
  }
  if (pressure >= 1040 && pressure < 1045) {
    if (trend == "Rising") return risingCode[1];
    if (trend == "Falling") return fallingCode[1];
    if (trend == "Steady") return steadyCode[1];
  }
  if (pressure >= 1035 && pressure < 1040) {
    if (trend == "Rising") return risingCode[2];
    if (trend == "Falling") return fallingCode[2];
    if (trend == "Steady") return steadyCode[2];
  }
  if (pressure >= 1030 && pressure < 1035) {
    if (trend == "Rising") return risingCode[3];
    if (trend == "Falling") return fallingCode[3];
    if (trend == "Steady") return steadyCode[3];
  }
  if (pressure >= 1025 && pressure < 1030) {
    if (trend == "Rising") return risingCode[4];
    if (trend == "Falling") return fallingCode[4];
    if (trend == "Steady") return steadyCode[4];
  }
  if (pressure >= 1020 && pressure < 1025) {
    if (trend == "Rising") return risingCode[5];
    if (trend == "Falling") return fallingCode[5];
    if (trend == "Steady") return steadyCode[5];
  }
  if (pressure >= 1015 && pressure < 1020) {
    if (trend == "Rising") return risingCode[6];
    if (trend == "Falling") return fallingCode[6];
    if (trend == "Steady") return steadyCode[6];
  }
  if (pressure >= 1010 && pressure < 1015) {
    if (trend == "Rising") return risingCode[7];
    if (trend == "Falling") return fallingCode[7];
    if (trend == "Steady") return steadyCode[7];
  }
  if (pressure >= 1005 && pressure < 1010) {
    if (trend == "Rising") return risingCode[8];
    if (trend == "Falling") return fallingCode[8];
    if (trend == "Steady") return steadyCode[8];
  }
  if (pressure >= 1000 && pressure < 1005) {
    if (trend == "Rising") return risingCode[9];
    if (trend == "Falling") return fallingCode[9];
    if (trend == "Steady") return steadyCode[9];
  }
  if (pressure >= 995 && pressure < 1000) {
    if (trend == "Rising") return risingCode[10];
    if (trend == "Falling") return fallingCode[10];
    if (trend == "Steady") return steadyCode[10];
  }
  if (pressure >= 990 && pressure < 995) {
    if (trend == "Rising") return risingCode[11];
    if (trend == "Falling") return fallingCode[11];
    if (trend == "Steady") return steadyCode[11];
  }
  if (pressure >= 985 && pressure < 990) {
    if (trend == "Rising") return risingCode[12];
    if (trend == "Falling") return fallingCode[12];
    if (trend == "Steady") return steadyCode[12];
  }
  if (pressure >= 980 && pressure < 985) {
    if (trend == "Rising") return risingCode[13];
    if (trend == "Falling") return fallingCode[13];
    if (trend == "Steady") return steadyCode[13];
  }
  if (pressure >= 975 && pressure < 980) {
    if (trend == "Rising") return risingCode[14];
    if (trend == "Falling") return fallingCode[14];
    if (trend == "Steady") return steadyCode[14];
  }
  if (pressure >= 970 && pressure < 975) {
    if (trend == "Rising") return risingCode[15];
    if (trend == "Falling") return fallingCode[15];
    if (trend == "Steady") return steadyCode[15];
  }
  if (pressure >= 965 && pressure < 970) {
    if (trend == "Rising") return risingCode[16];
    if (trend == "Falling") return fallingCode[16];
    if (trend == "Steady") return steadyCode[16];
  }
  if (pressure >= 960 && pressure < 965) {
    if (trend == "Rising") return risingCode[17];
    if (trend == "Falling") return fallingCode[17];
    if (trend == "Steady") return steadyCode[17];
  }
  if (pressure >= 955 && pressure < 960) {
    if (trend == "Rising") return risingCode[18];
    if (trend == "Falling") return fallingCode[18];
    if (trend == "Steady") return steadyCode[18];
  }
  if (pressure < 955) {
    if (trend == "Rising") return risingCode[19];
    if (trend == "Falling") return fallingCode[19];
    if (trend == "Steady") return steadyCode[19];
  }
  return "";
}
//#########################################################################################
//# A Winter falling generally results in a Z value lower by 1 unit compared with a Summer falling pressure.
//# Similarly a Summer rising, improves the prospects by 1 unit over a Winter rising.
//#          1050                           950
//#          |                               |
//# Rising   A A A A B B C F G I J L M Q T Y Y
//# Falling  A A A A B B D H O R U V X X Z Z Z
//# Steady   A A A A B B E K N P S W X X Z Z Z
String calc_zambretti(int zpressure, int month, float windDirection, float windSpeed, float Trend) {
  // Correct pressure for time of year
  zpressure = CorrectForWind(zpressure, OrdinalWindDir(windDirection), windSpeed);
  if (Trend <= -0.05) {              // FALLING
    if (month <= 3 || month >= 9) {  // Adjust for Winter
      zpressure -= 3.0;
    }
    return PressureToCode(zpressure, "Falling");
  }
  if (Trend >= 0.05) {               // RISING
    if (month <= 3 || month >= 9) {  // Adjust for Winter
      zpressure -= 3.0;
    }
    return PressureToCode(zpressure, "Rising");
  }
  if (Trend > -0.05 && Trend < 0.05) {  // STEADY
    return PressureToCode(zpressure, "Steady");
  }
  return "";
}
//#########################################################################################
String ZCode(String msg) {
  //Serial.println("MSG = " + msg);
  lcd.setSwapBytes(true);
  lcd.setSwapBytes(false);  // バイト順の変換を無効にする。
  int x_pos = 400;
  int y_pos = 255;
  int icon_x_size = 80;
  int icon_y_size = 80;
  String message = "";
  if (msg == "A") {
    //Icon = "https://openweathermap.org/img/wn/01d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_01d_2x);
    message = "Settled fine weather";
  }
  if (msg == "B") {
    //Icon = "https://openweathermap.org/img/wn/01d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_01d_2x);
    message = "Fine weather";
  }
  if (msg == "C") {
    //Icon = "https://openweathermap.org/img/wn/02d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_02d_2x);
    message = "Becoming fine";
  }
  if (msg == "D") {
    //Icon = "https://openweathermap.org/img/wn/02d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_02d_2x);
    message = "Fine, becoming less settled";
  }
  if (msg == "E") {
    //https://weather.metoffice.gov.uk/webfiles/latest/images/icons/weather/3.svg?19112020
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Fine, possible showers";
  }
  if (msg == "F") {
    //Icon = "https://openweathermap.org/img/wn/02d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_02d_2x);
    message = "Fairly fine, improving";
  }
  if (msg == "G") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Fairly fine, showers early";
  }
  if (msg == "H") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Fairly fine, showery later";
  }
  if (msg == "I") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Showery early, improving";
  }
  if (msg == "J") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_02d_2x);
    message = "Changeable, improving";
  }
  if (msg == "K") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Fairly fine, showers likely";
  }
  if (msg == "L") {
    //Icon = "https://openweathermap.org/img/wn/04d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_04d_2x);
    message = "Rather unsettled, clear later";
  }
  if (msg == "M") {
    //Icon = "https://openweathermap.org/img/wn/04d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_04d_2x);
    message = "Unsettled, probably improving";
  }
  if (msg == "N") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Showery, bright intervals";
  }
  if (msg == "O") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Showery, becoming unsettled";
  }
  if (msg == "P") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Changeable, some rain";
  }
  if (msg == "Q") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Unsettled, fine intervals";
  }
  if (msg == "R") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Unsettled, rain later";
  }
  if (msg == "S") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Unsettled, rain at times";
  }
  if (msg == "T") {
    //Icon = "https://openweathermap.org/img/wn/04d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_04d_2x);
    message = "Very unsettled, improving";
  }
  if (msg == "U") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Rain at times, worst later";
  }
  if (msg == "V") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Rain, becoming very unsettled";
  }
  if (msg == "W") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Rain at frequent intervals";
  }
  if (msg == "X") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Very unsettled, rain";
  }
  if (msg == "Y") {
    //Icon = "https://openweathermap.org/img/wn/11d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Stormy, may improve";
  }
  if (msg == "Z") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Stormy, much rain";
  }
  return message;
}
//#########################################################################################
float fmap(float sensorValue, float sensorMin, float sensorMax, float outMin, float outMax) {
  return (sensorValue - sensorMin) * (outMax - outMin) / (sensorMax - sensorMin) + outMin;
}
//#########################################################################################
// gauge(150, 420, temperature, RED, "Temperature", 0.8);
// gauge(150, 420, temperature, RED, "Temperature", 0.8);
void gauge(int x, int y, float value, int minValue, int maxValue, int lowStart, int highStart, String heading, float zoom, display_mode mode) {
  int low_colour, medium_colour, high_colour;
  float MinScale = 0;
  float MaxScale = 100;
  float start_angle = 120;
  float end_angle = 420;
  float Outer_diameter = 50;
  float Inner_diameter = 35;
  float old_value = value;
  String Unit = String(char(degcode));
  if (mode == _temperature || mode == _dewpoint || mode == _windchill) {
    low_colour = TFT_BLUE;
    medium_colour = TFT_GREEN;
    high_colour = TFT_RED;
    value = fmap(value, minValue, maxValue, MinScale, MaxScale);
  }
  if (mode == _humidity) {
    low_colour = TFT_RED;
    medium_colour = TFT_GREEN;
    high_colour = TFT_BLUE;
    value = fmap(value, minValue, maxValue, MinScale, MaxScale);
    Unit = "%";
  }
  if (mode == _battery) {
    low_colour = TFT_RED;
    medium_colour = TFT_GREEN;
    high_colour = TFT_BLUE;
    value = fmap(value, minValue, maxValue, MinScale, MaxScale);
    Unit = "%";
  }
  lcd.drawRoundRect(x - Outer_diameter * zoom - 10, y - Outer_diameter * zoom - 14, Outer_diameter * 2.5 * zoom, Outer_diameter * 2.5 * zoom, 12, TFT_YELLOW);
  lcd.drawArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle, end_angle, TFT_RED);
  if (value / MaxScale >= 0 && value / MaxScale <= lowStart / 100.0) {
    lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle, start_angle + (end_angle - start_angle) * value / MaxScale, low_colour);
  }
  if (value / MaxScale >= lowStart / 100.0) lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle, start_angle + (end_angle - start_angle) * lowStart / MaxScale, low_colour);
  if (value / MaxScale >= lowStart / 100.0 && value / MaxScale <= highStart / 100.0) {
    lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle + (end_angle - start_angle) * lowStart / MaxScale, start_angle + (end_angle - start_angle) * value / MaxScale, medium_colour);
  }
  if (value / MaxScale >= lowStart / 100.0 && value / MaxScale >= highStart / 100.0) lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle + (end_angle - start_angle) * lowStart / MaxScale, start_angle + (end_angle - start_angle) * highStart / MaxScale, medium_colour);
  if (value / MaxScale >= highStart / 100.0 && value / MaxScale <= MaxScale / 100.0) {
    lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle + (end_angle - start_angle) * highStart / MaxScale, start_angle + (end_angle - start_angle) * value / MaxScale, high_colour);
  }
  display_text(x - (heading.length() * 6.3) / 2, y - 70 * zoom, heading, TFT_WHITE, 1);
  display_text(x - (String(old_value, 1).length() * 7) / 2, y - 8 * zoom, String(old_value, 1) + Unit, TFT_WHITE, 1);
  if (mode == _temperature || mode == _dewpoint || mode == _windchill) {
    display_text(x + start_angle + (end_angle - start_angle) * lowStart / MaxScale - 228, y - start_angle + (end_angle - start_angle) * 0 / MaxScale + 117, "0", TFT_WHITE, 1);
    //display_text(x - Outer_diameter + 3, y - Outer_diameter + 48, "0", TFT_WHITE, 1);
    display_text(x - Outer_diameter + 8, y - Outer_diameter + 85, String(minValue), TFT_WHITE, 1);
    display_text(x - Outer_diameter + 75, y - Outer_diameter + 85, String(maxValue), TFT_WHITE, 1);
  }
  if (mode == _humidity || mode == _battery) {
    display_text(x - Outer_diameter + 20, y - Outer_diameter + 85, "0", TFT_WHITE, 1);
    display_text(x - Outer_diameter + 75, y - Outer_diameter + 85, "100", TFT_WHITE, 1);
  }
}
//#########################################################################################
void GetHAEntities(){
  GetEntityData("sensor.processor_temperature", &cputemperature);
  GetEntityData("sensor.pw3_soc", &soc);
  GetEntityData("sensor.met_office_melksham_temperature", &temperature);
  GetEntityData("sensor.met_office_melksham_humidity", &humidity);
  GetEntityData("sensor.met_office_melksham_uv_index", &uvindex);
  GetEntityData("sensor.met_office_melksham_feels_like_temperature", &feelslike);
  GetEntityData("sensor.met_office_melksham_probability_of_precipitation", &probprecip);
  GetEntityData("sensor.met_office_melksham_wind_speed", &windspeed);
  GetEntityData("sensor.met_office_melksham_wind_direction", &winddirection);
  GetEntityData("sensor.met_office_melksham_wind_gust", &windgust);
  GetEntityData("sensor.met_office_melksham_visibility_distance", &visibilitydistance);
  GetEntityData("sensor.met_office_melksham_weather", &weather);
  GetEntityData("weather.met_office_melksham", &metoweather);
  //GetEntityData("weather.met_office_melksham", &dewpoint);
}
// ###################### Get Entity Data #######################
void GetEntityData(String EntityName, String *Variable) {
  HTTPClient http;
  //Serial.println("Getting Entity Data...");
  String url     = "http://" + HomeAssistantIP + ":8123/api/states/" + EntityName;
  //Serial.println(url);
  String Auth    = "Bearer " + HAapi_key;
  String Content = "application/json";
  String Accept  = "application/json";
  String Payload = "";
  http.begin(url);  //Specify destination for HTTP request
  http.addHeader("Authorization", Auth);
  http.addHeader("Content-type", Content);
  http.addHeader("Accept", Accept);
  int httpResponseCode = http.GET();
  if (httpResponseCode >= 200) {
    //Serial.println("Received: " + String(httpResponseCode) + " as response Code");
    String response = http.getString();  //Get the response to the request
    //Serial.println(response);
    DecodeEntity(EntityName, response, Variable);
  } else Serial.println(httpResponseCode);
  http.end();
}
//#########################################################################################
void DecodeEntity(String EntityName, String input, String *Variable) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, input);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  //Serial.println(input);
  const char* entity_id           = doc["entity_id"]; // "sensor.windspeed"
  const char* state               = doc["state"]; // "5.0"  Serial.println(value);
  const char* unit_of_measurement = doc["attributes"]["unit_of_measurement"];
  const char* friendlyName        = doc["attributes"]["friendly_name"];
  dewpoint                        = String(doc["attributes"]["dew_point"]);
  pressure                        = String(doc["attributes"]["pressure"]);
  reading[0] = pressure.toFloat(); 
  *Variable = String(state);
  //Serial.println(String(friendlyName) + " = " + String(state) + String(unit_of_measurement) + " (" + EntityName + ")");
  //Serial.println(String(dewpoint) + " ~ " + String(pressure));
}  
