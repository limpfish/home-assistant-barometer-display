# home-assistant-barometer-display
 
A mini [Home Assistant](https://www.home-assistant.io/) display to show pressure &amp; temperature readings (and made to look pretty with 'freeform pcb' brass rods). It's powered by an esp8266 Wemos D1 Mini. Reads data via MQTT but would be an easy change to add a sensor directly.

![barometer-breadboard_s](https://user-images.githubusercontent.com/25790676/137713223-6d81d5dc-19c0-4ceb-99e3-4ab7a278103f.jpg)

![barometer-graph_s](https://user-images.githubusercontent.com/25790676/137713230-e4fc0cf0-3de9-4302-8332-297e7e719896.jpg)

Sensors dotted around the house, mostly BMP180 and SI7021, are pushing data to Home Assistant running [Mosquitto](https://github.com/home-assistant/addons/blob/master/mosquitto/DOCS.md) over MQTT.  Instead of having to load up the UI on my phone or PC, this is a permanent little display to show various sensor readings at a glance.  

A switch toggles the display between an old style antique barometer and a pressure graph.

RCWL-0516 microwave radar motion detector turns the screen off when no-one is around.

It uses a KMR-1.8 128 x 60 TFT screen connected over SPI despite the incorrectly marked I2C pins on the board (that took some figuring out).

The code  uses 
Adafruit's [ST7735](https://github.com/adafruit/Adafruit-ST7735-Library) and [GFX](https://github.com/adafruit/Adafruit-GFX-Library) libraries<br>
[ArduinoJson](https://arduinojson.org/) by Benoit Blanchon<br>
[PubSubClient](https://pubsubclient.knolleary.net/) by Nick O'Leary<br>
[ESP Wifi Manager](https://github.com/tzapu/WiFiManager) by Tzapu.

The main barometer graphic was designed in Photoshop at 128 x 160 and then converted to 565 bit RGB suitable for the ST7735 with [UTFTConverter](https://github.com/cirquit/UTFTConverter).

You'll need to setup your own MQTT server details:

    const char* mqtt_server = "192.168.0.160"; 
    const char* mqtt_user = "DVES_USER";
    const char* mqtt_pass = "DVES_PASS";

and  your MQTT topics, it assumes the 4th is pressure :

    char* mqtt_sub_topics[4][3] = 
    {
      //name               // mqtt topic          // unit  (unused here) 
      { "Outside",         "shed/info",           ""},
      { "Living Room",     "tele/sonoff5/SENSOR", ""},
      { "Attic",           "tele/sonoff4/SENSOR", ""},
      // 4th must be pressure
      { "Pressure",        "tele/sonoff1/SENSOR", ""}
    };


## Circuit diagram

![barometer-circuit-diagram_s](https://user-images.githubusercontent.com/25790676/137712211-d01c3f5f-b971-4f90-b4f7-ad8499efc868.png)
