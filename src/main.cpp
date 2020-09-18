#include <Arduino.h>

// Web Updater
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

// Post to InfluxDB
#include <ESP8266HTTPClient.h>

// Syslog
#include <WiFiUdp.h>
#include <Syslog.h>

// Analog read
#include <Adafruit_ADS1015.h>

#include "config.h"

#include <math.h> // for log()

#ifndef VERSION
  #define VERSION   NAME " 1.3 " __DATE__ " " __TIME__
#endif

// Syslog
WiFiUDP udp;
Syslog syslog(udp, SYSLOG_PROTO_IETF);

ESP8266WebServer web_server(PORT);

ESP8266HTTPUpdateServer esp_updater;

// Post to InfluxDB
WiFiClient client;
HTTPClient http;
int _influx_status = 0;

// Analog read samples
#define NUM_SENSORS 8
Adafruit_ADS1115 ads1(ADS1015_ADDRESS);
Adafruit_ADS1115 ads2(ADS1015_ADDRESS+1);
const uint16_t A_samples = A_SAMPLES;
const uint16_t A_max = A_MAX;
uint32_t _a_sum[NUM_SENSORS] = { 0 };         // sum of last analog reads
uint16_t _a_freq = 0;

// NTC characteristics (datasheet)
static const uint32_t B[NUM_SENSORS] = { NTC_B, NTC_B, NTC_B, NTC_B, NTC_B, NTC_B, NTC_B, NTC_B };
static const uint32_t R_n[NUM_SENSORS] = { NTC_R_N, NTC_R_N, NTC_R_N, NTC_R_N, NTC_R_N, NTC_R_N, NTC_R_N, NTC_R_N }; // Ohm
static const uint32_t T_n[NUM_SENSORS] = { NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N }; // Celsius

static const uint32_t R_v[NUM_SENSORS] = { NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N, NTC_T_N }; // Ohm, voltage divider resistor for NTC

uint32_t _r_ntc[NUM_SENSORS] = { 0 };         // Ohm, resistance updated with each analog read
double _temp_c[NUM_SENSORS] = { 0 };          // Celsius, calculated from NTC and R_v

// Temperature history
int16_t _t[NUM_SENSORS][8640/6];              // 1 day centicelsius temperature history in 60s intervals
uint16_t _t_pos[NUM_SENSORS];                 // position in history buffer


// Post data to InfluxDB
void post_data() {
  static const char Uri[]="/write?db=" INFLUX_DB "&precision=s";

  char msg[300]; 
  snprintf(msg, sizeof(msg), "temperatures zulauf=%.2f,ruecklauf=%.2f,vorlauf=%.2f,kamin=%.2f,frei1=%.2f,frei2=%.2f,frei3=%.2f,frei4=%.2f\n", 
    _temp_c[0], _temp_c[1], _temp_c[2], _temp_c[3], _temp_c[4], _temp_c[5], _temp_c[6], _temp_c[7]);
  http.begin(client, INFLUX_SERVER, INFLUX_PORT, Uri);
  http.setUserAgent(NAME);
  _influx_status = http.POST(msg);
  String payload = http.getString();
  http.end();
  if( _influx_status < 200 || _influx_status > 299 ) {
    snprintf(msg, sizeof(msg), "Post %s:%d%s status %d response '%s'", INFLUX_SERVER, INFLUX_PORT, Uri, _influx_status, payload.c_str());
    syslog.log(LOG_ERR, msg);
  };
}


// Default html menu page
void send_menu( const char *msg ) {
  static const char header[] = "<!doctype html>\n"
    "<html lang=\"en\">\n"
      "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"keywords\" content=\"MultiTemp, remote, meta\">\n"
        "<meta http-equiv=\"refresh\" content=\"10;url=/\">\n"
        "<title>MultiTemp Status</title>\n"
        "<style>\n"
          ".slidecontainer { width: 80%; }\n"
          ".slider {\n"
            "-webkit-appearance: none;\n"
            "width: 100%;\n"
            "height: 15px;\n"
            "border-radius: 5px;\n"
            "background: #d3d3d3;\n"
            "outline: none;\n"
            "opacity: 0.7;\n"
            "-webkit-transition: .2s;\n"
            "transition: opacity .2s; }\n"
          ".slider:hover { opacity: 1; }\n"
          ".slider::-webkit-slider-thumb {\n"
            "-webkit-appearance: none;\n"
            "appearance: none;\n"
            "width: 25px;\n"
            "height: 25px;\n"
            "border-radius: 50%;\n" 
            "background: #4CAF50;\n"
            "cursor: pointer; }\n"
          ".slider::-moz-range-thumb {\n"
            "width: 25px;\n"
            "height: 25px;\n"
            "border-radius: 50%;\n"
            "background: #4CAF50;\n"
            "cursor: pointer; }\n"
        "</style>\n"
      "</head>\n"
      "<body>\n"
        "<h1>MultiTemp NTC Status</h1>\n";
  static const char form[] = "<p>%s</p><table>\n"
          "<tr><th>Sensor</th><th>Temperature [&#8451;]</th><th>Resistance [&#8486;]</th><th>Analog</th></tr>\n"
          "<tr><th>1</th><th>%6.2f</th><td>%u</td><td>%u</td></tr>\n"
          "<tr><th>2</th><th>%6.2f</th><td>%u</td><td>%u</td></tr>\n"
          "<tr><th>3</th><th>%6.2f</th><td>%u</td><td>%u</td></tr>\n"
          "<tr><th>4</th><th>%6.2f</th><td>%u</td><td>%u</td></tr>\n"
          "<tr><th>5</th><th>%6.2f</th><td>%u</td><td>%u</td></tr>\n"
          "<tr><th>6</th><th>%6.2f</th><td>%u</td><td>%u</td></tr>\n"
          "<tr><th>7</th><th>%6.2f</th><td>%u</td><td>%u</td></tr>\n"
          "<tr><th>8</th><th>%6.2f</th><td>%u</td><td>%u</td></tr>\n"
        "</table>\n"
        "<p></p>\n"
        "<table>\n"
          "<tr><td>Frequency [Hz]</td><td>%u</td></tr>\n"
          "<tr><td>InfluxDB Status</td><td>%d</td></tr>\n"
          "<tr><td>Firmware</td><td>" VERSION "</td></tr>\n"
        "</table>\n";
        // "<p></p>\n"
        // "<form action=\"/calib\">\n"
        //   "<button>Calibrate</button>\n"
        // "</form>\n";
  static const char footer[] =
      "</body>\n"
    "</html>\n";
  static char page[sizeof(form)+100]; // form + variables

  size_t len = sizeof(header) + sizeof(footer) - 2;
  len += snprintf(page, sizeof(page), form, msg, 
    _temp_c[0], _r_ntc[0], _a_sum[0], 
    _temp_c[1], _r_ntc[1], _a_sum[1], 
    _temp_c[2], _r_ntc[2], _a_sum[2], 
    _temp_c[3], _r_ntc[3], _a_sum[3], 
    _temp_c[4], _r_ntc[4], _a_sum[4], 
    _temp_c[5], _r_ntc[5], _a_sum[5], 
    _temp_c[6], _r_ntc[6], _a_sum[6], 
    _temp_c[7], _r_ntc[7], _a_sum[7], 
    _a_freq, _influx_status);

  web_server.setContentLength(len);
  web_server.send(200, "text/html", header);
  web_server.sendContent(page);
  web_server.sendContent(footer);
}


// Initiate connection to Wifi but dont wait for it to be established
void setup_Wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(NAME);
  WiFi.begin(SSID, PASS);
  pinMode(ONLINE_LED_PIN, OUTPUT);
  digitalWrite(ONLINE_LED_PIN, HIGH);
}


// Define web pages for update, reset or for configuring parameters
void setup_Webserver() {

  // Call this page to see the ESPs firmware version
  web_server.on("/version", []() {
    send_menu(VERSION);
  });

  web_server.on("/temperature", []() {
    char msg[30];
    long n = 0;
    if( web_server.arg("n") != "" ) {
      n = web_server.arg("n").toInt();
    }
    switch( n ) {
      case 1:
        snprintf(msg, sizeof(msg), "%6.2f", _temp_c[0]);
        break;
      case 2:
        snprintf(msg, sizeof(msg), "%6.2f", _temp_c[1]);
        break;
      case 3:
        snprintf(msg, sizeof(msg), "%6.2f", _temp_c[2]);
        break;
      case 4:
        snprintf(msg, sizeof(msg), "%6.2f", _temp_c[3]);
        break;
      case 5:
        snprintf(msg, sizeof(msg), "%6.2f", _temp_c[4]);
        break;
      case 6:
        snprintf(msg, sizeof(msg), "%6.2f", _temp_c[5]);
        break;
      case 7:
        snprintf(msg, sizeof(msg), "%6.2f", _temp_c[6]);
        break;
      case 8:
        snprintf(msg, sizeof(msg), "%6.2f", _temp_c[7]);
        break;
      default:
        snprintf(msg, sizeof(msg), "%6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f", 
        _temp_c[0], _temp_c[1], _temp_c[2], _temp_c[3], _temp_c[4], _temp_c[5], _temp_c[6], _temp_c[7]);
        break;
    }
    web_server.send(200, "text/plain", msg);
  });

  // TODO crashes...
  web_server.on("/history.bin", []() {
    char msg[30];
    int len = snprintf(msg, sizeof(msg), "int16 centicelsius[%5u]:", sizeof(_t)/sizeof(*_t));
    web_server.setContentLength(CONTENT_LENGTH_UNKNOWN); // len + sizeof(t)
    web_server.send(200, "application/octet-stream", "");
      web_server.sendContent(msg, len);
    unsigned chunk = 1024;
    char *pos = (char *)_t;
    char *end = pos + sizeof(_t);
    while( pos < end - chunk ) {
      web_server.sendContent(pos, chunk);
      pos += chunk;
    }
    web_server.sendContent(pos, end - pos);
    web_server.sendContent("");
  });

  
  // Call this page to reset the ESP
  web_server.on("/reset", []() {
    syslog.log(LOG_NOTICE, "RESET");
    send_menu("Resetting...");
    delay(200);
    ESP.restart();
  });

  // Main page
  web_server.on("/", []() {
    char msg[80];
    snprintf(msg, sizeof(msg), "Welcome!");
    send_menu(msg);
  });

  // Catch all page, gives a hint on valid URLs
  web_server.onNotFound([]() {
    web_server.send(404, "text/plain", "error: use "
      "/reset, /version, /temperature, /history.bin or "
      "post image to /update\n");
  });

  web_server.begin();

  MDNS.addService("http", "tcp", PORT);
  syslog.logf(LOG_NOTICE, "Serving HTTP on port %d", PORT);
}


// Handle online web updater, initialize it after Wifi connection is established
void handleWifi() {
  static bool updater_needs_setup = true;
  static bool first_connect = true;

  if( WiFi.status() == WL_CONNECTED ) {
    if( first_connect ) {
      first_connect = false;
    }
    if( updater_needs_setup ) {
      // Init once after connection is (re)established
      digitalWrite(ONLINE_LED_PIN, LOW);
      Serial.printf("WLAN '%s' connected with IP ", SSID);
      Serial.println(WiFi.localIP());
      syslog.logf(LOG_NOTICE, "WLAN '%s' IP %s", SSID, WiFi.localIP().toString().c_str());

      MDNS.begin(NAME);

      esp_updater.setup(&web_server);
      setup_Webserver();

      Serial.println("Update with curl -F 'image=@firmware.bin' " NAME "/update");

      updater_needs_setup = false;
    }
    web_server.handleClient();
  }
  else {
    if( ! updater_needs_setup ) {
      // Cleanup once after connection is lost
      digitalWrite(ONLINE_LED_PIN, HIGH);
      updater_needs_setup = true;
      Serial.println("Lost connection");
    }
  }
}


void updateTemperature( const uint32_t r_ntc, double &temp_c, unsigned n ) {
  // only print if measurement decimals change 
  static int16_t t_prev[4] = { 0 };

  temp_c = 1.0 / (1.0/(273.15+T_n[n]) + log((double)r_ntc/R_n[n])/B[n]) - 273.15;

  int16_t temp = (int16_t)(temp_c * 100 + 0.5); // rounded centi celsius
  if( (temp - t_prev[n]) * (temp - t_prev[n]) >= 10 * 10 ) { // only report changes >= 0.1
    // Serial.printf("Temperature: %01d.%01d degree Celsius\n", temp/100, (temp/10)%10);
    t_prev[n] = temp;
  }
}


void updateResistance( const uint32_t a_sum, uint32_t &r_ntc, double &temp_c, unsigned n ) {
  static const uint32_t Max_sum = (uint32_t)A_max * A_samples;

  r_ntc = (int64_t)R_v[n] * a_sum / (Max_sum - a_sum);

  updateTemperature(r_ntc, temp_c, n);
}


void handleAnalog( uint32_t &a_sum, uint32_t &r_ntc, double &temp_c, unsigned n ) {
  static uint16_t a[4][A_samples] = { 0 }; // last analog reads
  static uint16_t a_pos[4] = { A_samples, A_samples, A_samples, A_samples }; // sample index

  uint16_t value;
  if( n < 4 ) {
    value = ads1.readADC_SingleEnded(n);
  }
  else {
    value = ads2.readADC_SingleEnded(n-4);
  }

  // first time init
  if( a_pos[n] == A_samples ) {
    while( a_pos[n]-- ) {
      a[n][a_pos[n]] = value;
      a_sum += value;
    }
  }
  else {
    if( ++a_pos[n] >= A_samples ) {
      a_pos[n] = 0;
    }

    a_sum -= a[n][a_pos[n]];
    a[n][a_pos[n]] = value;
    a_sum += a[n][a_pos[n]];
    // if( n == 3 ) Serial.printf("ads: %u %lu\n", value, a_sum);
  }

  updateResistance(a_sum, r_ntc, temp_c, n);
}


void handleFrequency() {
  static uint32_t start = 0;
  static uint32_t count = 0;

  count++;

  uint32_t now = millis();
  if( now - start > 1000 ) {
    // printf("Measuring analog at %u Hz\n", count);
    _a_freq = count;
    start = now;
    count = 0;
  }
}


void handleTempHistory( const double temp_c, int16_t t[], const uint16_t t_entries, uint16_t &t_pos, unsigned n ) {
  static bool first[4] = { true, true, true, true };
  uint16_t temp = (int16_t)(temp_c * 100 + 0.5); 
  if( first[n] ) {
    t_pos = t_entries;
    while( --t_pos > 0 ) {
      t[t_pos] = -30000; // mark as clearly invalid because below absolute zero
    }
    t[t_pos] = temp;
    first[n] = false;
  }
  else {
    if( ++t_pos >= t_entries ) {
      t_pos = 0;
    }
    t[t_pos] = temp;
  }
}


void setup() {
  Serial.begin(115200);

  // Initiate network connection (but dont wait for it)
  setup_Wifi();

  // ADS1115 init
  ads1.begin();
  ads1.begin();

  // Syslog setup
  syslog.server(SYSLOG_SERVER, SYSLOG_PORT);
  syslog.deviceHostname(NAME);
  syslog.appName("Joba1");
  syslog.defaultPriority(LOG_KERN);

  Serial.println("\nBooted " VERSION);
}

void loop() {
  handleFrequency();
  for( unsigned n = 0; n < NUM_SENSORS; n++ ) {
    handleAnalog(_a_sum[n], _r_ntc[n], _temp_c[n], n);
    handleTempHistory(_temp_c[n], _t[n], sizeof(_t[n])/sizeof(*_t[n]), _t_pos[n], n);
  }
  static uint32_t prev = 0;
  static uint16_t count = 0;
  uint32_t now = millis();
  if( now - prev > 1000 ) {
    static char msg[100];
    snprintf(msg, sizeof(msg), "Temp1-8 = %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f °C, %3u Hz", 
      _temp_c[0], _temp_c[1], _temp_c[2], _temp_c[3], _temp_c[4], _temp_c[5], _temp_c[6], _temp_c[7] , _a_freq);
    Serial.println(msg);
    if( WiFi.status() == WL_CONNECTED && count-- == 0 ) {
      syslog.log(LOG_INFO, msg);
      post_data();
      // snprintf(msg, sizeof(msg), "Rntc1-4 = %6u, %6u, %6u, %6u Ohm", _r_ntc[0], _r_ntc[1], _r_ntc[2], _r_ntc[3]);
      // syslog.log(LOG_INFO, msg);
      // snprintf(msg, sizeof(msg), "Asum1-4 = %6u, %6u, %6u, %6u", _a_sum[0], _a_sum[1], _a_sum[2], _a_sum[3]);
      // syslog.log(LOG_INFO, msg);
      count = 60; // 1 min until next syslog
    }
    prev = now;
  }
  handleWifi();
  delay(200); // don't heat up ntc
}