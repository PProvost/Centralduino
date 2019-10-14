#include <Arduino.h>
#include "ArduinoLog.h"
#include "Ticker.h"
#include "Centralduino.h"

// Used for random simluated telemetry values
const double minTemp = -20.0;
const double minLux = 0.0;

// Location of config.json on SPIFFS (must start with /)
const char *CONFIG_FILE = "/config.json";

// Forward declarations
bool reboot_callback();
void connectWifi();
void sendTelemetry();
void doReboot();

// Ticker setup - see https://github.com/sstaub/Ticker
Ticker telemetryTimer(sendTelemetry, 10000); // 10 seconds
Ticker rebootTimer(doReboot, 5000); // 5 sec

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
    Centralduino.setup(CONFIG_FILE);

    // Register a device method callback
    Centralduino.registerDeviceMethod("reboot", reboot_callback);

    Log.trace("Done setting up... starting timers." CR);

    Centralduino.sendProperty("firmware_ver", "1.1");

    // Start our callback timers
    telemetryTimer.start();
}

void loop()
{
    // Allow our timers to update
    telemetryTimer.update();
    rebootTimer.update();

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
    Centralduino.sendMeasurement("free_heap", ESP.getFreeHeap());
}

bool reboot_callback()
{
    // handle it!
    Log.notice("Rebooting in 5 sec!! ***********************");
    rebootTimer.start();
    return true;
}

void doReboot()
{
    Log.notice("doReboot() called" CR);
    ESP.restart();
}