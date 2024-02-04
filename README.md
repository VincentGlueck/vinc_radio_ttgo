# vinc_radio_ttgo
A TTGO Internet radio inspired by Volos projects

https://www.youtube.com/@VolosProjects

*TTGO/LILIGO board required, ESP32 required, Uno R3 or similar = no*

German alike, so most of the streams are provided by vendors near by.

_stations.h_

Overwrite to meet your needs.

As this prog runs in MONO (not STEREO) only PIN26 and GND must be connected as Cinch/3.5mm output. Voltage/level will fit - quite.

Sorry, bluetooth speakers currently not supported.

![ttgo_inet](https://github.com/VincentGlueck/vinc_radio_ttgo/assets/139572548/439c8ab1-e052-410b-81eb-4332a0f5cb14)

# Limitations

* shows dummy volume bar
* does not measure length of title correct (display issue)

# Credits
* audio, https://github.com/earlephilhower/ESP8266Audio
* title scroller inspired by mrdude2478, https://github.com/Bodmer/TFT_eSPI/discussions/1828
* Volos projects, https://www.youtube.com/@VolosProjects
