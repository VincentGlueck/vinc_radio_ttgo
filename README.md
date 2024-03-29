# vinc_radio_ttgo
A TTGO Internet radio inspired by Volos projects

https://www.youtube.com/@VolosProjects

*TTGO/LILIGO board required, ESP32 required, Uno R3 or similar = no*

German alike, so most of the streams are provided by vendors near by (Europe).

``stations.h``

Overwrite to meet your needs.

Connect at least PIN26 and GND as Cinch/3.5mm output (you may use stereo which is the default, 2nd channel: Pin 25).
No further resistors __needed__ but feel free to optimize sound. Do not ask which one is left or right - don't know...

Sorry, bluetooth speakers __not__ supported. This would cause a CPU load, ESP32 can't handle (my guess).

Currently, main loop() function must(!) at least call
```
handlePlay()
btn0.listen()
btn1.listen()
```

  
Do *not* use any ``delay(x)``!

__As of march 2024, UI is simplified to the max.__
Station and Volume are __one__. No labels. __Big__ time display. Title scroller, timer per title, done.

![raw_screen_simple](https://github.com/VincentGlueck/vinc_radio_ttgo/assets/139572548/b443f1e7-c5fd-4a99-8b6f-2d30cadfc777)
![IMG_20240314_174952490_HDR](https://github.com/VincentGlueck/vinc_radio_ttgo/assets/139572548/52ced723-668c-4f1b-9258-17fa702ced32)


``bg arts: (not this) https://creator.nightcafe.studio/u/Manncy``

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
* left-long: decrease volume
* left: stop playback

``Change behaviour by altering class ButtonCallback : public TtgoCallback in InternetRadio.ino``

# Limitations

* TtgoButton sometimes fails on double-click/-press

```
# Latest fixes/changes
* Taskplay.ino removed as it is in a too early state and probably will take lots of further investigation
* readPreferences() crashed on new device; getFloat("key") caused this -> changed to getInt("key")
```

# Credits
* audio, https://github.com/earlephilhower/ESP8266Audio
* title scroller inspired by mrdude2478, https://github.com/Bodmer/TFT_eSPI/discussions/1828
* preferences by Volodymyr Shymanskyy, https://github.com/vshymanskyy/Preferences
* Volos projects, https://www.youtube.com/@VolosProjects
* png/jpg -> .c file: http://www.rinkydinkelectronics.com/t_imageconverter565.php (unsecure, but ok)
* TTGO button handling, https://github.com/VincentGlueck/TTGO_buttons
