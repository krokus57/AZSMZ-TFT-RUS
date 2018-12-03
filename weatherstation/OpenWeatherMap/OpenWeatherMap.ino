/**The MIT License (MIT)
Copyright (c) 2017 by Daniel Eichhorn
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
See more at https://blog.squix.org
*/


/*****************************
 * Important: see settings.h to configure your settings!!!
 * ***************************/
#include "settings.h"

#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
//#include <time.h>
//#include <TimeLib.h>
int thermoCS = 1;
long get6675timer;

#include <SimpleDHT.h>
int pinDHT11 = 1;
SimpleDHT11 dht11;

byte TFT_CS;
byte TFT_DC;
bool HAVE_TOUCHPAD = true;
byte TOUCH_CS;
byte TOUCH_IRQ;
byte BTN_1;

// SPI_SETTING
#define SPI_SETTING     SPISettings(2000000, MSBFIRST, SPI_MODE0)
//#define SPI_SETTING     SPISettings(4000000, MSBFIRST, SPI_MODE0)

double getTemperatureCelcius() {
//  SPI.begin();
  uint16_t v;
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  delay(1);
  
  pinMode(thermoCS, OUTPUT);
  digitalWrite(thermoCS, HIGH);
  delay(1);
  digitalWrite(thermoCS, LOW);
  delay(1);
  digitalWrite(thermoCS, HIGH);
  delay(1); 
  digitalWrite(thermoCS, LOW);
  delay(1);

//  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI.beginTransaction(SPI_SETTING);

  v = SPI.transfer(0);
  v <<= 8;
  v |= SPI.transfer(0);

  SPI.endTransaction();
  digitalWrite(thermoCS, HIGH);

  if (v & 0x4) {
    // uh oh, a serious problem!
    return NAN;
  }

  if (v & 0x8000) {
    // Negative value, drop the lower 18 bits and explicitly extend sign bits.
    v = 0xFFC00000 | ((v >> 10) & 0x003FF);
  } else {
    // Positive value, just drop the lower 18 bits.
    v >>= 3;
  }

  double centigrade = v;

  // LSB = 0.25 degrees C
  centigrade *= 0.25;
  return centigrade;
}

int serialResistance;
int nominalResistance;
int bCoefficient;
int TEMPERATURENOMINAL;

#ifdef MUSIC
  #include <MmlMusicPWM.h>
  MmlMusicPWM music(3);
  void playMusic() {
    music.play("T120 L8 r eged e4 <ar > eged e4<ar> grgr g2 eedg e2 drdr d2 <ga>ed c2");
  }
#endif

  #include <XPT2046_Touchscreen.h>
  #include "TouchControllerWS.h"

//  #define VREF 2.5

  #define CFG_POWER  0b10100111
  #define CFG_TEMP0  0b10000111
  #define CFG_TEMP1  0b11110111
  #define CFG_AUX    0b11100111
  #define CFG_LIRQ   0b11010000
  #define CFG_IRQ    0b11010010
//  #define CFG_IRQ    0b11010000

  void XPT2046_setCFG(uint8_t c) {
    digitalWrite(TOUCH_CS, 0);
    delay(1);
//    SPI.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE0));
//    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    SPI.beginTransaction(SPI_SETTING);    
    const uint8_t buf[4] = { c, 0x00, 0x00, 0x00 };
    SPI.writeBytes((uint8_t *) &buf[0], 3);
    SPI.endTransaction();
    digitalWrite(TOUCH_CS, 1);
  }

  uint32_t XPT2046_ReadRaw(uint8_t c) {
    uint32_t p = 0;
    uint8_t i = 0;  
    digitalWrite(TOUCH_CS, 0);
    delay(1);
//    SPI.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE0));
//    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    SPI.beginTransaction(SPI_SETTING);    
    SPI.transfer16(c) >> 3;    
    for(; i < 10; i++) {
      delay(1);
      p += SPI.transfer16(c) >> 3;
    }
    p /= i;
    SPI.endTransaction();
    digitalWrite(TOUCH_CS, 1);
    XPT2046_setCFG(CFG_IRQ);        
    return p;
  }

int adjTemp;
bool NTC_INV = 0;

int readNTC() {
  XPT2046_setCFG(CFG_AUX); 
  delay(1);
  float average;
  if (NTC_INV) {
    average = serialResistance / ((4096 * 1.0 / XPT2046_ReadRaw(CFG_AUX)) - 1);
  } else {
    average = (4096 * 1.0 * serialResistance / XPT2046_ReadRaw(CFG_AUX)) - serialResistance;
  }
  
//  Serial.print("NTC R=");  
//  Serial.println(average);  

  average = average * (100 - adjTemp) / 100;
  
  float steinhart;
  steinhart = average * 1.0 / nominalResistance;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= bCoefficient;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
//  return (int)(steinhart * 10); 
  return (int)steinhart; 
}  
    /*
int ntc2temp(float r,int k){
  float steinhart;
  steinhart = r * 1.0 / k;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= bCoefficient;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
  return (int)(steinhart * 10); 
}  
*/

#include <JsonListener.h>
/*
#include <WundergroundConditions.h>
#include <WundergroundForecast.h>
#include <WundergroundAstronomy.h>
*/

#include <OpenWeatherMapCurrent.h>
#include <OpenWeatherMapForecast.h>
#include <Astronomy.h>

#include <MiniGrafx.h>
#include <carousel.h>
#include <ILI9341_SPI.h>

/*  
 *  if (hwSPI) spi_begin();   
    if (!(_rst>0)) writecommand(ILI9341_SWRESET);
    writecommand(0xEF);
    */


#include "fonts\ArialRounded.h"
//#include "ArialRounded.h"
#include "moonphases.h"
#include "weathericons.h"
#include "configportal.h"

#define MINI_BLACK 0
#define MINI_WHITE 1
#define MINI_YELLOW 2
#define MINI_BLUE 3

#define MAX_FORECASTS 8

// defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK, // 0
                      ILI9341_WHITE, // 1
                      ILI9341_YELLOW, // 2
                      0x7E3C
                      }; //3

int SCREEN_WIDTH = 240;
int SCREEN_HEIGHT = 320;
// Limited to 4 colors due to memory constraints
int BITS_PER_PIXEL = 2; // 2^2 =  4 colors

//#ifndef BATT
//  ADC_MODE(ADC_VCC);
//#endif

ILI9341_SPI *tft;
MiniGrafx *gfx;
Carousel *carousel;

//ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC);
//MiniGrafx gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette);
//Carousel carousel(&gfx, 0, 0, 240, 100);

XPT2046_Touchscreen *ts;
TouchControllerWS *touchController;

void calibrationCallback(int16_t x, int16_t y) {
  gfx->setColor(1);
  gfx->fillCircle(x, y, 10);
}

  void calibrationCallback(int16_t x, int16_t y);
  CalibrationCallback calibration = &calibrationCallback;
  void touchCalibration() {
//    Serial.println("Touchpad calibration .....");
    touchController->startCalibration(&calibration);
    while (!touchController->isCalibrationFinished()) {
      gfx->fillBuffer(0);
      gfx->setColor(MINI_YELLOW);
      gfx->setTextAlignment(TEXT_ALIGN_CENTER);
      gfx->drawString(120, 160, "Откалибруйте экран\nдотроньтесь\nдо точки");
      touchController->continueCalibration();
      gfx->commit();
      yield();
    }
    touchController->saveCalibration();
  }

/*
WGConditions conditions;
WGForecast forecasts[MAX_FORECASTS];
WGAstronomy astronomy;
*/

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
Astronomy::MoonData moonData;

// Setup simpleDSTadjust Library rules
simpleDSTadjust dstAdjusted(StartRule, EndRule);

void drawWifiQuality();
void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime(bool saver = false);
void drawForecast();
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
void drawAstronomy();
void drawCurrentWeatherDetail();
void drawLabelValue(uint8_t line, String label, String value);
void drawForecastTable(uint8_t start);
void drawAbout();
void drawSeparator(uint16_t y);
const char* getMeteoconIconFromProgmem(String iconText);
const char* getMiniMeteoconIconFromProgmem(String iconText);
void drawForecast1(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
void drawForecast2(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
FrameCallback frames[] = { drawForecast1, drawForecast2 };
int frameCount = 1;

// how many different screens do we have?
int screenCount = 5;
long lastDownloadUpdate = millis();

String moonAgeImage = "";
uint8_t moonAge = 0;
uint16_t screen = 0;
long timerPress;
long timerTouch;
bool canBtnPress;
bool btnClick;

void systemRestart() {
//    Serial.flush();
    delay(500);
//    Serial.swap();
    delay(100);
    ESP.restart();
    while (1) {
        delay(1);
    };
}

int getBtnState() {
    pinMode(BTN_1, INPUT_PULLUP);
    delay(2);
    int btnState = digitalRead(BTN_1);
    pinMode(BTN_1, OUTPUT);
    if (btnState == LOW){
      if(canBtnPress){
        timerPress = millis();
        canBtnPress = false;
      }else {
        if ((!btnClick) && ((millis() - timerPress)>3000)) {     // long press to pen init  
          return 2;
        }    
      }
    }else if(!canBtnPress){
      canBtnPress = true;
      btnClick = false;
      if ((millis() - timerPress)<800) {    
        return 1;
      }
    }   
    return 0;
}

bool firstConnect = true;

void restoreConfig() {
    int btnState = getBtnState();
    int i = 0;
    if (firstConnect && (btnState==1)) {
      showConfigMessage("Resume to default setting ?\n \n \nYes.(Long press Flash button)\n \nNo.(Short press Flash button)");
      while (true) {
        btnState = getBtnState();
        delay(200);
        if (btnState==1) { 
          /*
          if (i%3==0) BOARD = AZSMZ_1_1;
          if (i%3==1) BOARD = AZSMZ_1_6;
          if (i%3==2) BOARD = AZSMZ_1_8;
          setBoardType();        
          showConfigMessage("Resume to default setting ?\n \n \nYes.(Long press Flash button)\n \nNo.(Short press Flash button)");
          if (i++>5)
          */
          break;
        }
        if (btnState==2) {
          SPIFFS.remove(configFileName);
          showConfigMessage("\n \n \nSettings are restored. \n \nsystem goto restarting .....");
          delay(2000);
          systemRestart();             
        } 
        if ((millis() - timerPress)> 30 * 1000) break;
      }
    }
    if (btnState==2) {
      startConfig();
    }
}

boolean connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
    
  //Manual Wifi
  WiFi.mode(WIFI_STA);
  Serial.print("[");
  Serial.print(WIFI_SSID.c_str());
  Serial.print("]");
//  Serial.print("[");
//  Serial.print(WIFI_PASS.c_str());
//  Serial.print("]");
  
  WiFi.begin(WIFI_SSID.c_str(),WIFI_PASS.c_str());
  int i = 0; 
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    if (i > 200) {
      Serial.println("Не могу подключиться с WiFi");
      if (firstConnect) {
        firstConnect = false;
        startConfig();
      }
      return false;
    }
    if (!(i % 10)) drawProgress(i % 80 + 10,"Соединение с WiFi");    
    Serial.print(".");
    restoreConfig();
    i++;   
  }
  return true;
}

float power;

#define BATTERY_HIGH 4.18
#define BATTERY_LOW  3.7

int getPwPer() {
  if (power > BATTERY_HIGH) return 100;
  if (power < BATTERY_LOW)  return 0;
  return (power - BATTERY_LOW) * 100 / (BATTERY_HIGH - BATTERY_LOW);  
}

void powerOff() {
    if (getPwPer()<5) {
      gotoSleep("Battery low,please charge.");      
    }
}

#define MPW 10
  
void getPower() {
    if (HAVE_TOUCHPAD) {
      static int i;
      static float p = 0;
      //XPT2046_setCFG(CFG_POWER);
      if (!power) power = XPT2046_ReadRaw(CFG_POWER) * VREF * 4 / 4096;        
      if (!i) {
        power = p/MPW;
        p = 0;
        i = MPW;
      } else {
        i--;
        int v = XPT2046_ReadRaw(CFG_POWER);   
        p += v * VREF * 4 / 4096;
      }
    } else {
      power = analogRead(A0) * 49 / 10240.0;
    }
}

void setBoardType(){
  if (BOARD == AZSMZ_1_1) {
    TFT_DC = 5;
    TFT_CS = 4;
    BTN_1 = 0;
    #define SDA_PIN 0
    #define SCL_PIN 2  
    // LM75A Address
    #define Addr 0x48  
    #define BATT
  } else if (BOARD == AZSMZ_1_6 || BOARD == AZSMZ_1_8) {
    TFT_DC = 0;
    TFT_CS = 2;
    TOUCH_CS = 5;
    TOUCH_IRQ = 4;
    BTN_1 = 0;
//  #define VREF 3.3
    #define BATT
    bool USE_NTC = 0;
    NTC_INV = 0;
    if (BOARD == AZSMZ_1_8) NTC_INV = 1;
    HAVE_TOUCHPAD = 1;
    nominalResistance = 10;   // NTC 10K
    bCoefficient = 3950;      // B 3950
    TEMPERATURENOMINAL = 25;
    serialResistance = 10;
  //  #define MUSIC
  }

  initLCD();
    Serial.print("TFT_DC ");
    Serial.println(TFT_DC);
    Serial.print("TFT_CS ");
    Serial.println(TFT_CS);
    Serial.print("TFT_LED ");
    Serial.println(TFT_LED);
    Serial.print("BTN_1 ");
    Serial.println(BTN_1);
    Serial.print("SDA_PIN ");
    Serial.println(SDA_PIN);
    Serial.print("SCL_PIN ");
    Serial.println(SCL_PIN);
    Serial.print("TOUCH_CS ");
    Serial.println(TOUCH_CS);
    Serial.print("TOUCH_IRQ ");
    Serial.println(TOUCH_IRQ);
    Serial.print("HAVE_TOUCHPAD ");
    Serial.println(HAVE_TOUCHPAD);

    
}

//#include <Ticker.h>
//Ticker flipper;

//void flip()
//{
//  systemRestart();
//    gotoSleep("Going to Sleep!");
//    loop();  
//}

#include <ESP8266HTTPUpdateServer.h>
ESP8266HTTPUpdateServer httpUpdater;

#include <DNSServer.h>
const byte DNS_PORT = 53;
DNSServer dnsServer;

void initLCD() {
  tft = new ILI9341_SPI(TFT_CS, TFT_DC);  
  gfx = new MiniGrafx(tft, BITS_PER_PIXEL, palette);  
  carousel = new Carousel(gfx, 0, 0, 240, 100);
  carousel->disableAutoTransition();
  if (HAVE_TOUCHPAD) {
     ts = new XPT2046_Touchscreen(TOUCH_CS, TOUCH_IRQ);
     touchController = new TouchControllerWS(ts);
  }

//  gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette);  

  #define ILI9341_SWRESET 0x01
  tft->writecommand(ILI9341_SWRESET);
  delay(100);
  gfx->init(); 
    
  gfx->fillBuffer(MINI_BLACK);
  gfx->commit();
}

void setup() {
  Serial.begin(115200);
  WiFi.disconnect();
  Serial.println("Started!");
  pinMode(TFT_LED, OUTPUT);
  #ifdef TFT_LED_LOW
//    digitalWrite(TFT_LED, LOW); 
//    delay(500);  
    digitalWrite(TFT_LED, HIGH); 
    delay(300);  
    digitalWrite(TFT_LED, LOW);   
  #else
    digitalWrite(TFT_LED, HIGH);    // HIGH to Turn on;   
  #endif  

//  loadEepromConfig();
//  delay(100);
  BOARD = AZSMZ_1_8;

  setBoardType();
  
    delay(2000);
/*
  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft->readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft->readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft->readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft->readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft->readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX); 
  
  Serial.println(F("Benchmark                Time (microseconds)"));
  delay(10);
*/
  
    
  // load config if it exists. Otherwise use defaults.
  boolean mounted = SPIFFS.begin();
  if (!mounted) {
    SPIFFS.format();
    SPIFFS.begin();
  }
  // SPIFFS.remove(configFileName);
  loadConfig(); 
//  loadAdminConfig(); 

  timerPress = millis();
  canBtnPress = true;

  server.on ( "/", handleRoot );
  server.on ( "/save", handleSave);
  server.on ( "/admin", handleAdmin);  
  server.on ( "/adminsave", handleAdminSave);  
  server.on ( "/reset", []() {
//     ESP.restart();
      systemRestart();       
  } );
  server.onNotFound ( handleNotFound );

  httpUpdater.setup(&server);    
  server.begin();
  
  connectWifi();  

  if (HAVE_TOUCHPAD) { 
    ts->begin();
    //SPIFFS.remove("/calibration.txt");configFileName
    boolean isCalibrationAvailable = touchController->loadCalibration();
    if (!isCalibrationAvailable) {
      touchCalibration();
    } 
//  power = XPT2046_ReadRaw(CFG_POWER) * 2.5 * 4 / 4096;
//  getPower();
  }
  carousel->setFrames(frames, frameCount);
  carousel->disableAllIndicators();
 
  // update the weather information
  updateData();
}

long lastDrew = 0;
bool btnLongClick;

float temperature = 0.0;
int humidity = 0;

float C2F(float c) {
   return c * 1.8 + 32;
}

  #include <Wire.h>
  float lm75() {
    unsigned int data[2];
    Wire.begin(SDA_PIN,SCL_PIN);
    Wire.setClock(700000);
    // Start I2C Transmission
    Wire.beginTransmission(Addr);
    // Select temperature data register
    Wire.write(0x00);
    // Stop I2C Transmission
    Wire.endTransmission();
    // Request 2 bytes of data
    Wire.requestFrom(Addr,2);
    // Read 2 bytes of data
    // temp msb, temp lsb
    if(Wire.available()==2) {
      data[0] = Wire.read();
      data[1] = Wire.read();
    } 
    //  Serial.println(data[0]);  
    //  Serial.println(data[1]);
    // Convert the data to 9-bits
    int temp = (data[0] * 256 + (data[1] & 0x80)) / 128;
    if (temp > 255){
      temp -= 512;
    }
    float cTemp = temp * 0.5;
    return cTemp; 
    
    //float fTemp = cTemp * 1.8 + 32;
    // Output data to serial monitor
    //  Serial.print("Temperature in Celsius:  ");
    //  Serial.print(cTemp);
    //  Serial.println(" C");
    //  Serial.print("Temperature in Fahrenheit:  ");
    //  Serial.print(fTemp);
    //  Serial.println(" F");  
    //  if (IS_METRIC) 
    //    return cTemp; 
    //  else
    //    return fTemp;    
  }

void showConfigMessage(String s) {
  gfx->fillBuffer(0);  
  gfx->setColor(1);  
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setFont(ArialMT_Plain_16);
  gfx->drawString(240 / 2, 40,s);
  gfx->commit();  
}

void startConfig() {
  String s = "";
  if (WiFi.status() == WL_CONNECTED) {
      Serial.println ( "Open browser at http://" + WiFi.localIP().toString() );
      s = "AZSMZ TFT Setup Mode\nConnected to:\n" + WiFi.SSID() + "\nOpen browser at\nhttp://" + WiFi.localIP().toString();     
  } else {
      WiFi.mode(WIFI_AP);
      WiFi.softAP((ESP.getChipId()+CONFIG_SSID).c_str());
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());     
      IPAddress myIP = WiFi.softAPIP();  
//      Serial.println(myIP.toString());   
      s = "\nAZSMZ TFT Setup Mode\nConnect WiFi to:\n" + String(ESP.getChipId()) + CONFIG_SSID + "\nOpen browser at\nhttp://" + myIP.toString();
  }

  s += "\n \nPls short press again\n to exit setup.";
  if (HAVE_TOUCHPAD) {
    XPT2046_setCFG(CFG_POWER);     
    s += "\n \nPls long press again\n to setup TOUCHPAD.";
  }
  showConfigMessage(s);
  Serial.println ( "HTTP server started" );
  btnClick = true;
  btnLongClick = false;

  while(true) {
    if (WiFi.getMode()!=WIFI_STA ) {
        dnsServer.processNextRequest();
    }    
    server.handleClient();
    yield();

    int btnState =  getBtnState();
    if (btnState == 2) {
          if (HAVE_TOUCHPAD) {           
            touchCalibration(); 
            showConfigMessage(s);          
          }
    }
    if (btnState == 1) break;
    if ((millis() - timerPress)> 300 * 1000) break;
  }

  dnsServer.stop();
//  pinMode(BTN_1, OUTPUT);   
  updateData();
//  gfx->init();         
}

void gotoSleep(String message) {
      drawProgress(25,message);
      delay(1000);
      drawProgress(50,message);
      delay(1000);
      drawProgress(75,message);
      delay(1000);    
      drawProgress(100,message);
      // go to deepsleep for xx minutes or 0 = permanently
      if (HAVE_TOUCHPAD) XPT2046_setCFG(CFG_LIRQ);
      ESP.deepSleep(0,  WAKE_RF_DEFAULT);                       // 0 delay = permanently to sleep
}

bool isShowHumi = true;
void getTemp(){
    if (EXT_TEMP == ByNTC) {
      temperature = readNTC();      
    } else if (EXT_TEMP == ByDHT11) {
      static long dht_timer;
      static byte dht_temp = 0;
      static byte dht_humi = 0;
      if ((millis()-dht_timer)>1000) {
        dht11.read(pinDHT11, &dht_temp, &dht_humi, NULL);
        dht_timer = millis();
      } 
      if (isShowHumi) {
        temperature = (int)dht_temp ;  
      } else {
        humidity = (int)dht_humi;     
      }
    } else if (EXT_TEMP == ByMAX6675) {
      if ((millis()-get6675timer) > 2000) {
        get6675timer = millis();
        temperature = getTemperatureCelcius();
//        showConfigMessage(String(temperature)); return;
      }
    } else if (EXT_TEMP==ByLM75) {
      if (canBtnPress) temperature = lm75(); 
    }    
    if (!IS_METRIC) temperature = C2F(temperature);   
    if (EXT_TEMP==None) temperature = currentWeather.temp;
}

void loop() {
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  
if (!UPDATE_INTERVAL_MINS) UPDATE_INTERVAL_MINS = 1;
getPower();  
 
if (HAVE_TOUCHPAD) {
  digitalWrite(TFT_CS, HIGH);
  if (touchController->isTouched(200)) {
    TS_Point p = touchController->getPoint();
    if (screen==screenCount) screen=0;
    else {
      if (p.y < 80) {
        IS_STYLE_12HR = !IS_STYLE_12HR;
        if (screen == screenCount -1) gotoSleep("Спящий режим!");
      } else {
          screen = (screen + 1) % screenCount;
      }
    }
    timerTouch = millis();            
  }
}

if (BTN_1 !=-1) {
  
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  
  digitalWrite(BTN_1, 0);

  int btnState = getBtnState();
  if (btnState==1) {
    if (screen==screenCount) screen=0;
    else {
      screen = (screen + 1) % screenCount;
    }
  }
  if (btnState==2) {
    btnLongClick = true;
  } 
}

if (btnLongClick) {
  startConfig();          
  btnLongClick = false;
}

getTemp();      
//int t = ntc2temp(33 * 1024 / analogRead(A0) - 10 ,100) / 10;
//Serial.println(t);


//  if ((screen<screenCount) && ((millis() - timerPress)> 30 * 1000)) screen = 0;  // after 30 secs return screen 0
  if ((screen<screenCount) && ((millis() - timerPress)> 30 * 1000) && ((millis() - timerTouch)> 30 * 1000)) screen = 0;  // after 30 secs return screen 0

  gfx->fillBuffer(MINI_BLACK);
  if (screen == 0) {
    drawTime();
    //drawWifiQuality();
    #ifdef BATT
      //drawBattery();
    #endif     
    int remainingTimeBudget = carousel->update();
    if (remainingTimeBudget > 0) {
      // You can do some work here
      // Don't do stuff if you are below your
      // time budget.   
      delay(remainingTimeBudget);         
    }
    drawCurrentWeather();
    drawAstronomy();
  } else if (screen == 1) {
    drawCurrentWeatherDetail();
  } else if (screen == 2) {
    drawForecastTable(0);
  } else if (screen == 3) {
    drawForecastTable(4);
  } else if (screen == 4) {
    drawAbout();
  } else if (screen == screenCount) {
    drawTime(true);
  }
  gfx->commit();

  if (SLEEP_INTERVAL_SECS && (millis() - timerPress >= SLEEP_INTERVAL_SECS * 1000) && (millis() - timerTouch >= SLEEP_INTERVAL_SECS * 1000)){ // after 2 minutes go to sleep
    gotoSleep("Спящий режим!");
  }

  if (SAVER_INTERVAL_SECS && (millis() - timerPress >= SAVER_INTERVAL_SECS * 1000)&&(millis() - timerTouch >= SAVER_INTERVAL_SECS * 1000)){ // after SAVER_INTERVAL_SECS go to saver
      screen = screenCount;
  }
  
  // Check if we should update weather information
  if (millis() - lastDownloadUpdate > 1000 * UPDATE_INTERVAL_MINS * 60) {
      updateData();
      lastDownloadUpdate = millis();
  } 
  
}

time_t dstOffset = 0;

// Update the internet based information and update screen
void updateData() {
  if (!connectWifi()) return;

//  flipper.attach(5, flip); // 30s 
  
  gfx->fillBuffer(MINI_BLACK);
  gfx->setFont(ArialRoundedMTBold_14);

  drawProgress(10, "Обновление времени...");
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
  while(!time(nullptr)) {
    Serial.print("#");
    delay(100);
  }
  // calculate for time calculation how much the dst class adds.
  dstOffset = UTC_OFFSET * 3600 + dstAdjusted.time(nullptr) - time(nullptr);
  Serial.printf("Time difference for DST: %d", dstOffset);

  drawProgress(50, "Обновление погоды...");
  OpenWeatherMapCurrent *currentWeatherClient = new OpenWeatherMapCurrent();
  currentWeatherClient->setMetric(IS_METRIC);
  currentWeatherClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
//  currentWeatherClient->updateCurrent(&currentWeather, OPEN_WEATHER_MAP_APP_ID,  Weather_CITY + "," + Weather_COUNTRY);
  #ifdef OPEN_WEATHER_MAP_LOCATION_ID
    currentWeatherClient->updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID,  String(OPEN_WEATHER_MAP_LOCATION_ID));
  #else
    currentWeatherClient->updateCurrent(&currentWeather, OPEN_WEATHER_MAP_APP_ID,  Weather_CITY + "," + Weather_COUNTRY);
  #endif
  
  delete currentWeatherClient;
  currentWeatherClient = nullptr;

  drawProgress(70, "Обновление прогноза...");
  OpenWeatherMapForecast *forecastClient = new OpenWeatherMapForecast();
  forecastClient->setMetric(IS_METRIC);
  forecastClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12, 0};
  forecastClient->setAllowedHours(allowedHours, sizeof(allowedHours));

  #ifdef OPEN_WEATHER_MAP_LOCATION_ID
    forecastClient->updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);
  #else
    forecastClient->updateForecasts(forecasts, OPEN_WEATHER_MAP_APP_ID, Weather_CITY + "," + Weather_COUNTRY, MAX_FORECASTS);
  #endif
  
  //forecastClient->updateForecasts(forecasts, OPEN_WEATHER_MAP_APP_ID, Weather_CITY + "," + Weather_COUNTRY, MAX_FORECASTS);
  delete forecastClient;
  forecastClient = nullptr;

  drawProgress(80, "Обновление астрономии...");
  Astronomy *astronomy = new Astronomy();
  moonData = astronomy->calculateMoonData(time(nullptr));
  float lunarMonth = 29.53;
  moonAge = moonData.phase <= 4 ? lunarMonth * moonData.illumination / 2 : lunarMonth - moonData.illumination * lunarMonth / 2;
  moonAgeImage = String((char) (65 + ((uint8_t) ((26 * moonAge / 30) % 26))));
  delete astronomy;
  astronomy = nullptr;
  delay(1000);

/*  
  drawProgress(10, "Updating time...");
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);

  drawProgress(50, "Updating conditions...");
  WundergroundConditions *conditionsClient = new WundergroundConditions(IS_METRIC);
  conditionsClient->updateConditions(&conditions, WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  delete conditionsClient;
  conditionsClient = nullptr;

  drawProgress(70, "Updating forecasts...");
  WundergroundForecast *forecastClient = new WundergroundForecast(IS_METRIC);
  forecastClient->updateForecast(forecasts, MAX_FORECASTS, WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  delete forecastClient;
  forecastClient = nullptr;

  drawProgress(80, "Updating astronomy...");
  WundergroundAstronomy *astronomyClient = new WundergroundAstronomy(IS_STYLE_12HR);
  astronomyClient->updateAstronomy(&astronomy, WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  delete astronomyClient;
  astronomyClient = nullptr;
  moonAgeImage = String((char) (65 + 26 * (((15 + astronomy.moonAge.toInt()) % 30) / 30.0)));

//  flipper.detach();
  delay(1000);
  */
}

// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
  gfx->fillBuffer(MINI_BLACK);
  gfx->drawPalettedBitmapFromPgm(23, 30, SquixLogo);
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setColor(MINI_WHITE);
  gfx->drawString(120, 80, "https://blog.squix.org");
  gfx->setColor(MINI_YELLOW);

  gfx->drawString(120, 146, text);
  gfx->setColor(MINI_WHITE);
  gfx->drawRect(10, 168, 240 - 20, 15);
  gfx->setColor(MINI_BLUE);
  gfx->fillRect(12, 170, 216 * percentage / 100, 11);

  gfx->commit();
}

String time_prev;
long timeSaver;

// draws the clock
void drawTime(bool saver) {
  static int x;
  static int y;
  if (!saver) {
    x = 120;
    y = 1;
  }  
  char *dstAbbrev;
  char time_str[11];
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  if (IS_STYLE_12HR) {
    int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
    sprintf(time_str, "%2d:%02d:%02d\n",hour, timeinfo->tm_min, timeinfo->tm_sec);
  } else {
//    sprintf(time_str, "%02d:%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    sprintf(time_str, "%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min);
  }

  if (saver && (!time_prev.equals(time_str) || (millis() - timeSaver > 1000))) {
    x = random(75,130);
    y = random(270);
    time_prev = time_str;
    timeSaver = millis();    
  } 

  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setColor(MINI_WHITE);
  char date[30];
  sprintf(date, "%s %2d %s %2d г.", name_week[timeinfo->tm_wday], timeinfo->tm_mday, name_month[timeinfo->tm_mon], timeinfo->tm_year-2000);
  //gfx->drawString(x, y, date);  
  
  
  char date_str[30];
  if (timeinfo->tm_mday>9)
    sprintf(date_str, "%s, %2d %s", name_week[timeinfo->tm_wday], timeinfo->tm_mday, name_month_short[timeinfo->tm_mon]);
  else
    sprintf(date_str, "%s, %1d %s", name_week[timeinfo->tm_wday], timeinfo->tm_mday, name_month_short[timeinfo->tm_mon]);

  gfx->setColor(MINI_BLUE);  
  gfx->setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  gfx->drawString(120, 55, date_str);
  
  gfx->setColor(MINI_WHITE);
  gfx->setFont(ArialRoundedMTBold_36);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->drawString(120, y+4, time_str);
  
}

// draws current weather information
void drawCurrentWeather() {
  gfx->setTransparentColor(MINI_BLACK);
  gfx->drawPalettedBitmapFromPgm(0, 40, getMeteoconIconFromProgmem(currentWeather.icon));
  // Weather Text

  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setColor(MINI_BLUE);
  gfx->setTextAlignment(TEXT_ALIGN_RIGHT);
//  gfx->drawString(220, 65, DISPLAYED_CITY_NAME);
//  gfx->drawString(220, 65, WUNDERGROUND_CITY);
  gfx->drawString(239, 62, DISPLAYED_CITY_NAME);

  gfx->setFont(ArialRoundedMTBold_36);
  gfx->setColor(MINI_WHITE);
  gfx->setTextAlignment(TEXT_ALIGN_RIGHT);
  
  String degreeSign = "°F";
  if (IS_METRIC) {
    degreeSign = "°C";
  }

  isShowHumi = false;    
  static long showHumiTimer;
  if (millis()-showHumiTimer<1000){
      gfx->drawString(239, 76, (int)temperature + degreeSign);  
  } else if (EXT_TEMP==ByDHT11) {
      isShowHumi = true;
      gfx->drawString(239, 76, String(humidity) + "%");  
      if (millis()-showHumiTimer>2000) showHumiTimer = millis();
  } else {
      gfx->drawString(239, 76, (int)temperature + degreeSign);    
  }
  
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setColor(MINI_YELLOW);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->drawString(120, 123, currentWeather.description);

}

void drawForecast1(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
  drawForecastDetail(x + 10, y + 165, 0);
  drawForecastDetail(x + 95, y + 165, 2);
  drawForecastDetail(x + 180, y + 165, 4);
}

void drawForecast2(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
  drawForecastDetail(x + 10, y + 165, 6);
  drawForecastDetail(x + 95, y + 165, 8);
  drawForecastDetail(x + 180, y + 165, 10);
}


// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {
  gfx->setColor(MINI_YELLOW);
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  time_t time = forecasts[dayIndex].observationTime + dstOffset;
  struct tm * timeinfo = localtime (&time);
  gfx->drawString(x + 25, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(timeinfo->tm_hour) + ":00");

  gfx->setColor(MINI_WHITE);
  gfx->drawString(x + 25, y, String(forecasts[dayIndex].temp, 1) + (IS_METRIC ? "°C" : "°F"));

  gfx->drawPalettedBitmapFromPgm(x, y + 15, getMiniMeteoconIconFromProgmem(forecasts[dayIndex].icon));
  gfx->setColor(MINI_BLUE);
  gfx->drawString(x + 25, y + 60, String(forecasts[dayIndex].rain, 1) + (IS_METRIC ? "мм" : "in"));
}
/*
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {
  gfx->setColor(MINI_YELLOW);
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  String day = forecasts[dayIndex].forecastTitle.substring(0, 3);
  day.toUpperCase();
  gfx->drawString(x + 25, y - 15, day);

  gfx->setColor(MINI_WHITE);
  gfx->drawString(x + 25, y, forecasts[dayIndex].forecastLowTemp + "|" + forecasts[dayIndex].forecastHighTemp);

  gfx->drawPalettedBitmapFromPgm(x, y + 15, getMiniMeteoconIconFromProgmem(forecasts[dayIndex].forecastIcon));
  gfx->setColor(MINI_BLUE);
  gfx->drawString(x + 25, y + 60, forecasts[dayIndex].PoP + "%");
}
*/

//tm *gmtime(time_t *timestamp) {
//  char *dstAbbrev;
////  char time_str[11];
//  time_t now = dstAdjusted.time(&dstAbbrev);
//  struct tm * timeinfo = localtime (&now);
//  return timeinfo; 
//}

String getTime(time_t *ts) {
  struct tm *timeInfo = gmtime(ts);
  
  char buf[6];
  sprintf(buf, "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buf);
}

// draw moonphase and sunrise/set and moonrise/set
void drawAstronomy() {

  gfx->setFont(MoonPhases_Regular_36);
  gfx->setColor(MINI_WHITE);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->drawString(120, 275, moonAgeImage);

  gfx->setColor(MINI_WHITE);
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setColor(MINI_YELLOW);
  gfx->drawString(120, 252, MOON_PHASES[moonData.phase]);
  
  gfx->setTextAlignment(TEXT_ALIGN_LEFT);
  gfx->setColor(MINI_YELLOW);
  gfx->drawString(5, 252, "Солнце");
  gfx->setColor(MINI_WHITE);
  time_t time_rise = currentWeather.sunrise + dstOffset;

  gfx->drawString(5, 275, "Восх:");
  gfx->drawString(45, 275, getTime(&time_rise));
  time_t time_set = currentWeather.sunset + dstOffset;
  gfx->drawString(5, 292, "Зак:");
  gfx->drawString(45, 292, getTime(&time_set));

  //Serial.printf("%d,%d, ",currentWeather.sunrise,time_rise);
  //Serial.println(getTime(&time_rise));
  //Serial.printf("%d,%d, ",currentWeather.sunset,time_set);
  //Serial.println(getTime(&time_set));

  gfx->setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx->setColor(MINI_YELLOW);
  gfx->drawString(235, 252, "Луна");
  gfx->setColor(MINI_WHITE);
  gfx->drawString(235, 275, String(moonAge) + "д");
  gfx->drawString(235, 292, String(moonData.illumination * 100, 0) + "%");
  gfx->drawString(200, 275, "Возр:");
  gfx->drawString(200, 292, "Ярк:");

}

/*
void drawAstronomy() {
  gfx->setFont(MoonPhases_Regular_36);
  gfx->setColor(MINI_WHITE);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->drawString(120, 275, moonAgeImage);

  gfx->setColor(MINI_WHITE);
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setColor(MINI_YELLOW);
  gfx->drawString(120, 250, astronomy.moonPhase);
  gfx->setTextAlignment(TEXT_ALIGN_LEFT);
  gfx->setColor(MINI_YELLOW);
  gfx->drawString(5, 250, "Sun");
  gfx->setColor(MINI_WHITE);
  gfx->drawString(5, 276, astronomy.sunriseTime);
  gfx->drawString(5, 291, astronomy.sunsetTime);

  gfx->setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx->setColor(MINI_YELLOW);
  gfx->drawString(235, 250, "Moon");
  gfx->setColor(MINI_WHITE);
  gfx->drawString(235, 276, astronomy.moonriseTime);
  gfx->drawString(235, 291, astronomy.moonsetTime);
}
*/

void drawCurrentWeatherDetail() {
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setColor(MINI_WHITE);
  gfx->drawString(120, 2, "Погода сейчас");

  //gfx->setTransparentColor(MINI_BLACK);
  //gfx->drawPalettedBitmapFromPgm(0, 20, getMeteoconIconFromProgmem(conditions.weatherIcon));

  String degreeSign = "°F";
  if (IS_METRIC) {
    degreeSign = "°C";
  }
  // String weatherIcon;
  // String weatherText;
  /*
  drawLabelValue(0, "Temperature:", conditions.currentTemp + degreeSign);
  drawLabelValue(1, "Feels Like:", conditions.feelslike + degreeSign);
  drawLabelValue(2, "Dew Point:", conditions.dewPoint + degreeSign);
  drawLabelValue(3, "Wind Speed:", conditions.windSpeed);
  drawLabelValue(4, "Wind Dir:", conditions.windDir);
  drawLabelValue(5, "Humidity:", conditions.humidity);
  drawLabelValue(6, "Pressure:", conditions.pressure);
  drawLabelValue(7, "Precipitation:", conditions.precipitationToday);
  drawLabelValue(8, "UV:", conditions.UV);

  gfx->setTextAlignment(TEXT_ALIGN_LEFT);
  gfx->setColor(MINI_YELLOW);
  gfx->drawString(15, 185, "Description: ");
  gfx->setColor(MINI_WHITE);
  gfx->drawStringMaxWidth(15, 200, 240 - 2 * 15, forecasts[0].forecastText);
  */

  drawLabelValue(0, "Температура:", currentWeather.temp + degreeSign);
  drawLabelValue(1, "Ветер:", String(currentWeather.windSpeed, 1) + (IS_METRIC ? "м/с" : "mph") );
  drawLabelValue(2, "Направление ветра:", String(currentWeather.windDeg, 1) + "°");
  drawLabelValue(3, "Влажность:", String(currentWeather.humidity) + "%");
  drawLabelValue(4, "Давление:", String(currentWeather.pressure*0.75006156130264) + "мм.рт.с");
  drawLabelValue(5, "Облачность:", String(currentWeather.clouds) + "%");
  drawLabelValue(6, "Видимость:", String(currentWeather.visibility) + "м");
  
}

void drawLabelValue(uint8_t line, String label, String value) {
  const uint8_t labelX = 15;
  const uint8_t valueX = 165;
  gfx->setTextAlignment(TEXT_ALIGN_LEFT);
  gfx->setColor(MINI_YELLOW);
  gfx->drawString(labelX, 30 + line * 15, label);
  gfx->setColor(MINI_WHITE);
  gfx->drawString(valueX, 30 + line * 15, value);
}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if(dbm <= -100) {
      return 0;
  } else if(dbm >= -50) {
      return 100;
  } else {
      return 2 * (dbm + 100);
  }
}

void drawWifiQuality() {
  int8_t quality = getWifiQuality();
  gfx->setColor(MINI_WHITE);
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_RIGHT);  
  gfx->drawString(228, 9, String(quality) + "%");
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        gfx->setPixel(230 + 2 * i, 18 - j);
      }
    }
  }
}

void drawBattery() {
  uint8_t percentage = getPwPer();  
  gfx->setColor(MINI_WHITE);
  gfx->setTextAlignment(TEXT_ALIGN_LEFT);  
  gfx->drawString(26, 9, String(percentage) + "%");
  gfx->drawRect(1, 11, 18, 9);
  gfx->drawLine(21,13,21,17);  
  gfx->drawLine(22,13,22,17);  
  gfx->setColor(MINI_BLUE); 
  gfx->fillRect(3, 13, 15 * percentage / 100, 5);
}

void drawForecastTable(uint8_t start) {
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setColor(MINI_WHITE);
  gfx->drawString(120, 2, "Прогноз погоды");
  uint16_t y = 0;

  String degreeSign = "°F";
  if (IS_METRIC) {
    degreeSign = "°C";
  }
  for (uint8_t i = start; i < start + 4; i++) {
    gfx->setTextAlignment(TEXT_ALIGN_LEFT);
    y = 45 + (i - start) * 75;
    if (y > 320) {
      break;
    }
    gfx->setColor(MINI_WHITE);
    gfx->setTextAlignment(TEXT_ALIGN_LEFT);
    time_t time = forecasts[i].observationTime + dstOffset;
    struct tm * timeinfo = localtime (&time);
    gfx->drawString(0, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(timeinfo->tm_hour) + ":00");

   
    gfx->drawPalettedBitmapFromPgm(0, 5 + y, getMiniMeteoconIconFromProgmem(forecasts[i].icon));
    
    
    gfx->setTextAlignment(TEXT_ALIGN_LEFT);
    gfx->setColor(MINI_YELLOW);
    gfx->setFont(ArialRoundedMTBold_14);
    gfx->setTextAlignment(TEXT_ALIGN_LEFT);
    //gfx->drawString(10, y, forecasts[i].main);
    gfx->drawString(60, y - 15, forecasts[i].description);
    //Serial.print(forecasts[i].description);

    gfx->setTextAlignment(TEXT_ALIGN_LEFT);
   
    gfx->setColor(MINI_BLUE);
    gfx->drawString(50, y, "Т:");
    gfx->setColor(MINI_WHITE);
    gfx->drawString(70, y, String(forecasts[i].temp, 0) + degreeSign);
    
    gfx->setColor(MINI_BLUE);
    gfx->drawString(50, y + 15, "В:");
    gfx->setColor(MINI_WHITE);
    gfx->drawString(70, y + 15, String(forecasts[i].humidity) + "%");

    gfx->setColor(MINI_BLUE);
    gfx->drawString(50, y + 30, "Д: ");
    gfx->setColor(MINI_WHITE);
    gfx->drawString(70, y + 30, String(forecasts[i].rain, 2) + (IS_METRIC ? "мм" : "in"));

    gfx->setColor(MINI_BLUE);
    gfx->drawString(130, y, "Дав:");
    gfx->setColor(MINI_WHITE);
    gfx->drawString(170, y, String(forecasts[i].pressure*0.75006156130264, 0) + "мм.рт.с");
    
    gfx->setColor(MINI_BLUE);
    gfx->drawString(130, y + 15, "Вет:");
    gfx->setColor(MINI_WHITE);
    gfx->drawString(170, y + 15, String(forecasts[i].windSpeed, 0) + (IS_METRIC ? "м/с" : "mph") );

    gfx->setColor(MINI_BLUE);
    gfx->drawString(130, y + 30, "Нап: ");
    gfx->setColor(MINI_WHITE);
    gfx->drawString(170, y + 30, String(forecasts[i].windDeg, 0) + "°");

  }
}

/*
void drawForecastTable(uint8_t start) {
  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setColor(MINI_WHITE);
  gfx->drawString(120, 2, "Forecasts");
  uint16_t y = 0;
  String degreeSign = "°F";
  if (IS_METRIC) {
    degreeSign = "°C";
  }
  for (uint8_t i = start; i < start + 6; i++) {
    gfx->setTextAlignment(TEXT_ALIGN_LEFT);
    y = 30 + (i - start) * 45;
    if (y > 320) {
      break;
    }
    gfx->drawPalettedBitmapFromPgm(0, y, getMiniMeteoconIconFromProgmem(forecasts[i].forecastIcon));
    gfx->setColor(MINI_YELLOW);
    gfx->setFont(ArialRoundedMTBold_14);
    gfx->drawString(50, y, forecasts[i].forecastTitle);
    gfx->setColor(MINI_WHITE);
    gfx->drawString(50, y + 15, getShortText(forecasts[i].forecastIcon));
    gfx->setColor(MINI_WHITE);
    gfx->setTextAlignment(TEXT_ALIGN_RIGHT);
    String temp = "";
    if (i % 2 == 0) {
      temp = forecasts[i].forecastHighTemp;
    } else {
      temp = forecasts[i - 1].forecastLowTemp;
    }
    gfx->drawString(235, y, temp + degreeSign);
    
    // *gfx->setColor(MINI_WHITE);
    // gfx->drawString(x + 25, y, forecasts[dayIndex].forecastLowTemp + "|" + forecasts[dayIndex].forecastHighTemp);
    // gfx->drawPalettedBitmapFromPgm(x, y + 15, getMiniMeteoconIconFromProgmem(forecasts[dayIndex].forecastIcon)); * /
    gfx->setColor(MINI_BLUE);
    gfx->drawString(235, y + 15, forecasts[i].PoP + "%");
  }
}
*/

void drawAbout() {
  gfx->fillBuffer(MINI_BLACK);
  gfx->drawPalettedBitmapFromPgm(23, 30, SquixLogo);

  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
  gfx->setColor(MINI_WHITE);
  gfx->drawString(120, 80, "https://blog.squix.org");

  gfx->setFont(ArialRoundedMTBold_14);
  gfx->setTextAlignment(TEXT_ALIGN_CENTER);
//  drawLabelValue(7, "Оперативная память:", String(ESP.getFreeHeap() / 1024)+"kb");
  drawLabelValue(7, "Свободная память:", String(ESP.getFreeHeap() / 1024)+"kb");
  drawLabelValue(8, "Постоянная память:", String(ESP.getFlashChipRealSize() / 1024 / 1024) + "MB");
  drawLabelValue(9, "Мощность WiFi:", String(WiFi.RSSI()) + "dB");
  drawLabelValue(10, "ИД чипа:", String(ESP.getChipId()));
  
  //#ifdef BATT
    //drawLabelValue(11, "Battery: ", String(power) +"V");
  //#else
  drawLabelValue(11, "Напряжение: ", String(ESP.getVcc() / 1024.0) +"В");
  //#endif     
  drawLabelValue(12, "Частота CPU.: ", String(ESP.getCpuFreqMHz()) + "МГц");
  drawLabelValue(13, "Город: ", currentWeather.cityName);
  char time_str[15];
  const uint32_t millis_in_day = 1000 * 60 * 60 * 24;
  const uint32_t millis_in_hour = 1000 * 60 * 60;
  const uint32_t millis_in_minute = 1000 * 60;
  uint8_t days = millis() / (millis_in_day);
  uint8_t hours = (millis() - (days * millis_in_day)) / millis_in_hour;
  uint8_t minutes = (millis() - (days * millis_in_day) - (hours * millis_in_hour)) / millis_in_minute;
  sprintf(time_str, "%2dd%2dh%2dm", days, hours, minutes);
  drawLabelValue(14, "Время работы: ", time_str);
  gfx->setTextAlignment(TEXT_ALIGN_LEFT);
  gfx->setColor(MINI_YELLOW);
  //gfx->drawString(15, 250, "Last Reset: ");
  //gfx->setColor(MINI_WHITE);
  //gfx->drawStringMaxWidth(15, 265, 240 - 2 * 15, ESP.getResetInfo());
}

/*
// Helper function, should be part of the weather station library and should disappear soon
const char* getMeteoconIconFromProgmem(String iconText) {
  if (iconText == "chanceflurries") return chanceflurries;
  if (iconText == "chancerain") return chancerain;
  if (iconText == "chancesleet") return chancesleet;
  if (iconText == "chancesnow") return chancesnow;
  if (iconText == "chancetstorms") return chancestorms;
  if (iconText == "clear") return clear;
  if (iconText == "cloudy") return cloudy;
  if (iconText == "flurries") return flurries;
  if (iconText == "fog") return fog;
  if (iconText == "hazy") return hazy;
  if (iconText == "mostlycloudy") return mostlycloudy;
  if (iconText == "mostlysunny") return mostlysunny;
  if (iconText == "partlycloudy") return partlycloudy;
  if (iconText == "partlysunny") return partlysunny;
  if (iconText == "sleet") return sleet;
  if (iconText == "rain") return rain;
  if (iconText == "snow") return snow;
  if (iconText == "sunny") return sunny;
  if (iconText == "tstorms") return tstorms;
  return unknown;
}

const char* getMiniMeteoconIconFromProgmem(String iconText) {
  if (iconText == "chanceflurries") return minichanceflurries;
  if (iconText == "chancerain") return minichancerain;
  if (iconText == "chancesleet") return minichancesleet;
  if (iconText == "chancesnow") return minichancesnow;
  if (iconText == "chancetstorms") return minichancestorms;
  if (iconText == "clear") return miniclear;
  if (iconText == "cloudy") return minicloudy;
  if (iconText == "flurries") return miniflurries;
  if (iconText == "fog") return minifog;
  if (iconText == "hazy") return minihazy;
  if (iconText == "mostlycloudy") return minimostlycloudy;
  if (iconText == "mostlysunny") return minimostlysunny;
  if (iconText == "partlycloudy") return minipartlycloudy;
  if (iconText == "partlysunny") return minipartlysunny;
  if (iconText == "sleet") return minisleet;
  if (iconText == "rain") return minirain;
  if (iconText == "snow") return minisnow;
  if (iconText == "sunny") return minisunny;
  if (iconText == "tstorms") return minitstorms;
  return miniunknown;
}
*/

// Helper function, should be part of the weather station library and should disappear soon
const String getShortText(String iconText) {
  if (iconText == "chanceflurries") return "Chance of Flurries";
  if (iconText == "chancerain") return "Chance of Rain";
  if (iconText == "chancesleet") return "Chance of Sleet";
  if (iconText == "chancesnow") return "Chance of Snow";
  if (iconText == "chancetstorms") return "Chance of Storms";
  if (iconText == "clear") return "Clear";
  if (iconText == "cloudy") return "Cloudy";
  if (iconText == "flurries") return "Flurries";
  if (iconText == "fog") return "Туман";
  if (iconText == "hazy") return "Hazy";
  if (iconText == "mostlycloudy") return "Mostly Cloudy";
  if (iconText == "mostlysunny") return "Mostly Sunny";
  if (iconText == "partlycloudy") return "Partly Couldy";
  if (iconText == "partlysunny") return "Partly Sunny";
  if (iconText == "sleet") return "Sleet";
  if (iconText == "rain") return "Дождь";
  if (iconText == "snow") return "Снег";
  if (iconText == "sunny") return "Sunny";
  if (iconText == "tstorms") return "Storms";
  return "-";
}
