/**The MIT License (MIT)
Copyright (c) 2015 by Daniel Eichhorn
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
See more at http://blog.squix.ch
*/

#include <simpleDSTadjust.h>

// Config mode SSID
const String CONFIG_SSID = "@AZSMZ_TFT";

// Setup
String WIFI_SSID = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
String WIFI_PASS = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

int UPDATE_INTERVAL_MINS = 30; // Update every 30 minutes
int SAVER_INTERVAL_SECS = 0 * 60;   // Going to screen saver after idle times, set 0 for dont screen saver.
int SLEEP_INTERVAL_SECS = 0 * 60;   // Going to Sleep after idle times, set 0 for dont sleep.

#define None      0
#define ByLM75    1
#define ByNTC     2
#define ByDHT11   3
#define ByMAX6675 4

float VREF = 2.5;
int EXT_TEMP = None;

// #define DHT11
#define SQUIX         10
#define AZSMZ_1_1     11
#define AZSMZ_1_6     16    // AZSMZ TFT ver 1.6 (have touchpad or no touchpad)
#define AZSMZ_1_8     18

//#define BOARD SQUIX
//#define BOARD AZSMZ_1_1
byte BOARD = AZSMZ_1_8;

#define TFT_LED 16
#define TFT_LED_LOW       // set LOW to Turn on;

// Wunderground Settings
// To check your settings first try them out in your browser:
// http://api.wunderground.com/api/WUNDERGROUND_API_KEY/conditions/q/WUNDERGROUND_COUNTTRY/WUNDERGROUND_CITY.json
// e.g. http://api.wunderground.com/api/808b********4511/conditions/q/CH/Zurich.json
// e.g. http://api.wunderground.com/api/808b********4511/conditions/q/CA/SAN_FRANCISCO.json <- note that in the US you use the state instead of country code

//String DISPLAYED_CITY_NAME = "Zürich";
//String WUNDERGRROUND_API_KEY = "7ac907535fc9a1ff";
//String WUNDERGRROUND_LANGUAGE = "EN";
String Weather_COUNTRY = "RU";
String Weather_CITY = "Moscow";
//String Weather_CITY = "Удельная";

String OPEN_WEATHER_MAP_APP_ID = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

//#define OPEN_WEATHER_MAP_LOCATION_ID 479640; //479640-Удельная
String OPEN_WEATHER_MAP_LOCATION  = Weather_CITY+","+Weather_COUNTRY;
String OPEN_WEATHER_MAP_LANGUAGE = "ru";
String DISPLAYED_CITY_NAME = "Удельная";

// Adjust according to your language
const String WDAY_NAMES[] = {"Вc", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
const String MONTH_NAMES[] = {"Янв", "Фев", "Мар", "Апр", "Май", "Июн", "Июл", "Авг", "Сен", "Окт", "Ноя", "Дек"};
const String MOON_PHASES[] = {"Новолуние", "Растущий серп", "Первая четверть", "Прибывающая луна",
                              "Полнолуние", "Убывающая луна", "Последняя четверть ", "Старая луна"};

static const char* name_week [] = {"Вc", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
static const char* name_month[] = {"Января", "Февраля", "Марта", "Апреля", "Мая", "Июня", "Июля", "Августа", "Сентября", "Октября", "Ноября", "Декабря"};
static const char* name_month_short[] = {"янв", "фев", "мар", "апр", "мая", "июн", "июл", "авг", "сен", "окт", "ноя", "дек"};


int UTC_OFFSET = 3;
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour

// Settings for Boston
// #define UTC_OFFSET -5
// struct dstRule StartRule = {"EDT", Second, Sun, Mar, 2, 3600}; // Eastern Daylight time = UTC/GMT -4 hours
// struct dstRule EndRule = {"EST", First, Sun, Nov, 1, 0};       // Eastern Standard time = UTC/GMT -5 hour

// values in metric or imperial system?
bool IS_METRIC = true;

// Change for 12 Hour/ 24 hour style clock
bool IS_STYLE_12HR = false;

// change for different ntp (time servers)
//#define NTP_SERVERS "0.ch.pool.ntp.org", "1.ch.pool.ntp.org", "2.ch.pool.ntp.org"
#define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"

/***************************
 * End Settings
 **************************/
