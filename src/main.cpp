#include <Arduino.h>
#include <ArduinoLog.h>
#include <Ticker.h>
#include <centralduino.h>

// Used for random simluated telemetry values
const double minTemp = -20.0;
const double minLux = 0.0;

// Forward declarations
void reboot_callback();
void connectWifi();
void sendTelemetry();

// Ticker setup - see https://github.com/sstaub/Ticker
Ticker timer1(sendTelemetry, 10000); // every 10 seconds

void setup()
{
    // Setup Serial and Logging first
    Serial.begin(115200);
    Serial.println();
    
    // Centralduino uses ArduinoLog internally, so we need to set it up and specify the 
    // verbosity. If you want to completely disable logging, see the following doc:
    // https://github.com/thijse/Arduino-Log#disable-library
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);

    // Required to be called in your setup() function
    // Will ensure the network is setup if it isn't already
    Centralduino.setup();

    // Register a device method callback
    Centralduino.registerDeviceMethod("reboot", reboot_callback);

    // Start our callback timers
    timer1.start();
}

void loop()
{
    // Allow our timers to update
    timer1.update();

    // Always call this at the end of loop()
    Centralduino.loop();
}

void sendTelemetry()
{
    // Send a measurement
    double temp = minTemp + (rand() % 10);
    double lux = minLux + (rand() % 10);

    Centralduino.sendMeasurement("temp", temp);
    Centralduino.sendMeasurement("lux", lux);
}

void reboot_callback()
{
    // handle it!
    
    ESP.restart();
}

