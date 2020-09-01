#ifndef CONFIG_H
#define CONFIG_H

// Network stuff, might already be defined by the build tools
#ifdef WLANCONFIG
  #include <WlanConfig.h>
#endif
#ifndef SSID
  #define SSID      WlanConfig::Ssid
#endif
#ifndef PASS
  #define PASS      WlanConfig::Password
#endif
#ifndef NAME
  #define NAME      "MultiTemp"
#endif
#ifndef PORT
  #define PORT      80
#endif

// InfluxDB server connection info
#define INFLUX_SERVER "192.168.1.4"
#define INFLUX_PORT   8086
#define INFLUX_DB     "heating"

// Syslog server connection info
#define SYSLOG_SERVER "192.168.1.4"
#define SYSLOG_PORT 514

#define ONLINE_LED_PIN   D4

// Calibration
#define B_DIFF 16
#define V_DIFF 114
#define R_DIFF -100

// Analog samples for averaging
#define A_SAMPLES        10
// default gain 2/3: Vmax=6.144V, Vcc = 3.3-0.111 => 32767*3.189/6.144 
#define A_MAX            (32767*(3300-V_DIFF)/6144)

// NTC parameters and voltage divider resistor
#define NTC_B            (3950+B_DIFF)
#define NTC_R_N          100000
#define NTC_T_N          25
#define NTC_R_V          (33000+R_DIFF)      

#endif
