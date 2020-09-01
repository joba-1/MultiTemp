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

// Syslog server connection info
#define SYSLOG_SERVER "192.168.1.4"
#define SYSLOG_PORT 514

#define ONLINE_LED_PIN   D4

// Analog samples for averaging
#define A_SAMPLES        1000
// default gain 2/3: Vmax=6.144V => 32767*3.3/6.144 
#define A_MAX            17600

// NTC parameters and voltage divider resistor
#define NTC_B            3950
#define NTC_R_N          100000
#define NTC_T_N          25
#define NTC_R_V          32900              

#endif
