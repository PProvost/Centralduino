#include "config.h"
#include <FS.h>
#include <ArduinoJson.h>
#include <ArduinoLog.h>

bool CentralduinoConfigClass::loadConfig(const char *path)
{
    DynamicJsonDocument doc(2048);

    if (!SPIFFS.begin())
    {
        Log.error("Failed to mount SPIFFS filesystem. Unable to continue." CR);
        return false;
    }

    if (!SPIFFS.exists(path))
    {
        Log.error("Config file not found in SPIFFS! Unable to continue." CR);
        return false;
    }

    File file = SPIFFS.open(path, "r");
    DeserializationError error = deserializeJson(doc, file);

    if (error)
    {
        Log.error("Failed to read config file. Unable to continue." CR);
        return false;
    }

    strlcpy(network.ssid, doc["network"]["ssid"], sizeof(network.ssid));
    strlcpy(network.password, doc["network"]["password"], sizeof(network.password));

    strlcpy(hub.device_id, doc["hub"]["device_id"], sizeof(hub.device_id));
    strlcpy(hub.scope_id, doc["hub"]["scope_id"], sizeof(hub.scope_id));
    strlcpy(hub.sas_key, doc["hub"]["sas_key"], sizeof(hub.sas_key));

    file.close();

    return true;
}

void CentralduinoConfigClass::dumpConfigToLog()
{
    Log.trace("*** BEGIN CONFIG ***" CR);
    Log.trace("network.ssid: %s" CR, network.ssid);
    Log.trace("network.password: %s" CR, network.password);
    Log.trace("hub.device_id: %s" CR, hub.device_id);
    Log.trace("hub.scope_id: %s" CR, hub.scope_id);
    Log.trace("hub.sas_key: %s" CR, hub.sas_key);
    Log.trace("*** END CONFIG ***" CR);
}

// Allocate the singleton
CentralduinoConfigClass CentralduinoConfig;
