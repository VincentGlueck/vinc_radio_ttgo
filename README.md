# vinc_radio_ttgo
A TTGO Internet radio inspired by Volos projects

https://www.youtube.com/@VolosProjects

*TTGO/LILIGO board required, ESP32 required, Uno R3 or similar = no*

German alike, so most of the streams are provided by vendors near by.

_stations.h_

Overwrite to meet your needs.

As this prog runs in MONO (not STEREO) only PIN26 and GND must be connected as Cinch/3.5mm output. Voltage/level will fit - quite.

Sorry, bluetooth speakers currently not supported.

Currently, main loop() function must(!) at least call
* ``HandlePlay()``
* ``btn0.listen()``
* ``btn1.listen()``
  
Do *not* use any ``delay(x)``!

![ttgo_radio](https://github.com/VincentGlueck/vinc_radio_ttgo/assets/139572548/7bd122b4-38e4-4f22-b8ac-d5cafff77351)

``bg arts: https://creator.nightcafe.studio/u/Manncy``

# Basic key functions

__When paused__:
* left: start playback
* left-long: decrease volume
* right: switch to next station
* right-long: increase volume
* right-double: switch to previous station

__When playing__:
* right: increase volume
* right-long: go on with increasing volume
* right-double: decrease volumne
* left: stop playback

``Change behaviour by altering classes ButtonCallback0/1 : public TtgoCallback in InternetRadio.ino``

# Limitations

* TtgoButton lib needs improvements (long press causes single press, too etc.)

# Credits
* audio, https://github.com/earlephilhower/ESP8266Audio
* title scroller inspired by mrdude2478, https://github.com/Bodmer/TFT_eSPI/discussions/1828
* Volos projects, https://www.youtube.com/@VolosProjects
* png/jpg -> .c file: http://www.rinkydinkelectronics.com/t_imageconverter565.php (unsecure, but ok)
