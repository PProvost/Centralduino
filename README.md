# Centralduino
A sample app and framework for very simple, quick development of IoT Devices. NOTE: Currently only supports ESP8266 but others will be added in the future or can be adapted as needed.

## SSL/TLS Performance & MCU Clock Speed

Since Azure IoT Hub and Azure DPS require SSL/TLS for the connections, make sure you're hardware can handle the crypto
stuff. An ESP8266, for example, should have the clock speed turned up from 80MHz to 160MHz.

## TODO
* Find whatever is causing my WDT resets
* Remove/replace the StringBuffer class
* Support direct methods, r/w properties (aka settings)
* Too many other things to list at this point, but the basic shape of it works
