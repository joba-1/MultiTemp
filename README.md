# Measure Multiple Temperatures

use an ESP8266, an ADS1115 and four NTCs to monitor heating system

## Status

Work in progress.

* OTA is working
* Syslog works
* Temperature measurement works
* Webpage with temperatures works
* Post to InfluxDB works
* Theory for temperature measuring is done (see below). Needs calibration to accomodate tolerances.

## Todo (maybe)

* Webpages for calibration (currently calibration done manually and hardcoded in config.h)
* MQTT publish temps
* EEPROM calibration data

## NTC Temperature Measurement

Formula for getting temperature in K from measuring Rntc

    T = 1 / (1/Tn + ln(Rt/Rn)/B)  (1)

* Tn = Temperature where Rntc = Rn. Usually 25°C -> 298.15K, but check datasheet to be sure.
* Rn = NTC Resistance at Tn. Usually 10k .. 100k. Better measure yourself, can differ quite a bit from datasheet.
* B = B-constant from datasheet. Often 3950 K.
* Rt = measured resistance

    Rt,min = Rn * e^(B/Tmin - B/Tn)  with Tmin = 0°C   -> Rt,min ~ 340k
    Rt,max = Rn * e^(B/Tmax - B/Tn)  with Tmax = 260°C -> Rt,max ~ 290

### Using an ADS1115

Using an ADS1115 you can compare Vcc to Vntc like the following sketch.
Since using Va3 as Varef === 0 it eliminates errors at Vax due to Vcc fluctuations.
Comparing Ax to A3 with A3 === 0 gives Ax ~ 0 for very high Rntc and Ax ~ -Amax for very low Rtc.

    Vcc ---+--- Rv ---+--- Rntc --- Gnd 
           |          |
           A3         Ax (Vcc=0...-32667=Gnd)

Since I need all 4 Ax and accuracy is good enough, I'll not use A3 as Vref but fixed Vmax = 6.144V

    Vcc --- Rv ---+--- Rntc --- Gnd 
                  |
                  Ax (Vcc=0...17600=Gnd) (17600 = 32767/6.144V*3.3V)

Voltage divider and some algebra with ohms law gives

Rntc = -Rv * (1 + Amax/Ax) (2)

Sanity check: 
If Rv chosen to be same as Rn, then Vntc at Ax should be Vcc/2 at 25°C.
Vcc/2 should result in Ax = -Amax/2. Put this Ax in (2) yields Rntc = Rv --> as expected.
Put this Rntc as Rt in (1) gives T = 1 / (1/298.15 + 1/3950*ln(1)) = 298.15K = 25°C --> as expected.

Choosing Rv: 
100k is good for low to medium range up to 150°C. 
10k gives ~10x better resolution at high temperatures around 250°C.
33k gives best resolution around 50°C +/- 50°C (delta A / 1°C > 500, so my Rv will be near 33k.

### Using ESP8266 ADC via NodeMCU A0

This method is less accurate, but maybe good enough for some usecase:

                              ADC
                               |
                  +--- 220k ---+--- 100k --- Gnd
                  |                                 NodeMCU board
                  A0 (Vcc=1023...0=Gnd)            --------------------
                  |                                 My voltage divider
    Vcc --- Rv ---+--- Rntc --- Gnd

Calculation

    Vcc/(Rv+Rntc) = Vntc/Rntc                | * (Rv+Rntc) * Rntc
    -> Vcc * Rntc = Vntc * Rv + Vntc * Rntc  | - (Vntc * Rntc)        
    -> Rntc * (Vcc-Vntc) = Rv * Vntc         | / (Vcc-Vntc)
    -> Rntc = Rv * Vntc/(Vcc-Vntc)           | * (1/Vcc)/(1/Vcc)
    -> Rntc = Rv * Vntc/Vcc/(1 - Vntc/Vcc)   | Vntc/Vcc = A0/Amax (Amax = 1023)
    -> Rntc = Rv * A0/1023 / (1-A0/1023)     | * (1023/1023)
    -> Rntc = Rv * A0 / (1023 - A0)

Solve for A0 to do some test calculations: 

       Rntc = Rv * A0 / (1023 - A0)       | * (1023 - A0)
    -> Rntc * 1023 - Rntc * A0 = Rv * A0  | + (Rntc * A0)
    -> Rntc * 1023 = (Rv + Rntc) * A0     | / (Rv + Rntc)
    -> Rntc * 1023 / (Rv + Rntc) = A0 

## Used Hardware

* [LEORX NTC 3950 100k Thermistoren mit Teflon 5 PCS](https://www.amazon.de/dp/B01AA7U82C?ref=ppx_pop_mob_ap_share) (mine had B=3966 - which is within tolerance)
* [ADS1115 16-bit ADC 4-channel analog/digital converter](https://www.amazon.de/dp/B07S9RH1MQ/ref=cm_sw_r_em_apa_i_G.3tFbC2Z5NFAADS1115)
* Any ESP8266 board you like (I used a NodeMCU V3 with onboard voltage divider at pin A0 which I needed for testing)
* 4 * 33k resistors (low tolerance preferred - easier or even unneccesary to calibrate)
