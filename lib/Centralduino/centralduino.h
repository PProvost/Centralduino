#ifndef __FRAMEWORK_H
#define __FRAMEWORK_H

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

// The PubSubClient library has a really small default max packet
// size (512 bytes). Unfortuntely, you can't simply override it
// here (see the github issues for more info). There are two options:
//
// 1. Change the following defines in PubSubClient.h
// #define MQTT_MAX_PACKET_SIZE 1024 
// #define MQTT_SOCKET_TIMEOUT 20
//
// 2. If you are using a tool that lets you configure build flags
// (e.g. platformio), the most reliable solution is to add a build
// flag to set these defines. 
// 
// Here is the line for your platformio.ini file
// build_flags = -DMQTT_MAX_PACKET_SIZE=1024 -DMQTT_SOCKET_TIMEOUT=20
// See https://docs.platformio.org/en/latest/projectconf/section_env_build.html
#include <PubSubClient.h>

#include "string_buffer.h"

typedef std::function<void()> MethodCallbackFunctionType;

// Public API functions here
class CentralduinoClass
{
  public:
    void setup(const char* configFilePath);
    void sendMeasurement(const char *name, double value);
    void registerDeviceMethod(const char *name, MethodCallbackFunctionType callback);
    void loop();

  private:
    void sendTwinUpdateRequest();
    void ensureWiFiConnected();
    void syncNtpTime();
    void ensureHubConnected();
    void registerCallbacks();
    int getUsernameAndPasswordFromConnectionString(const char *connectionString, size_t connectionStringLength,
                                                                      StringBuffer &hostName, StringBuffer &deviceId,
                                                                      StringBuffer &username, StringBuffer &password);

  private:
    bool _isHubConnected;
    StaticJsonDocument<1024> _jsonDocument;
    PubSubClient _mqttClient;
    WiFiClientSecure _wifiClient;
};

// Declare the global singleton
extern CentralduinoClass Centralduino;

#endif // __FRAMEWORK_H