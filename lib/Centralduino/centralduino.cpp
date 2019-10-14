#include "centralduino.h"
#include <FS.h>
#include <ArduinoLog.h>
#include <assert.h>

#include "defines.h"
#include "config.h"
#include "string_buffer.h"
#include "azure_dps.h"

#define MAX_REGISTERED_METHODS 10
typedef struct tagMethodRegistration
{
    const char *name;
    MethodCallbackFunctionType callback;
} MethodRegistrationEntry;

PubSubClient _mqttClient;
WiFiClientSecure _wifiClient;

MethodRegistrationEntry methodRegistry[MAX_REGISTERED_METHODS];

void CentralduinoClass::setup(const char *configFilePath)
{
    Log.notice(CR "********* Centralduino starting *********" CR);
    delay(1000);

    CentralduinoConfig.loadConfig(configFilePath);
    CentralduinoConfig.dumpConfigToLog();

    ensureWiFiConnected();
    syncNtpTime(); // setup the internal NTP stuff
    ensureHubConnected();
    registerCallbacks();
    sendTwinUpdateRequest();
}

void CentralduinoClass::loop()
{
    // Log.trace("Heap free: %d" CR, ESP.getFreeHeap());
    ensureHubConnected();
    _mqttClient.loop();
}

void CentralduinoClass::sendProperty(const char *name, const char *value)
{
    char topic[128];
    int rid = 123; // TODO
    sprintf(topic, PROPERTY_TOPIC_FMT, CentralduinoConfig.hub.device_id, rid);

    StaticJsonDocument<MQTT_MAX_PACKET_SIZE> payload;
    payload[name] = value;

    char buffer[MQTT_MAX_PACKET_SIZE];
    serializeJson(payload, buffer);
    Log.trace("MQTT Publishing to %s" CR, topic);
    Log.trace("Payload %s" CR, buffer);
    _mqttClient.publish(topic, buffer);
}

void CentralduinoClass::sendMeasurement(const char *name, double value)
{
    char topic[128]; // TODO
    sprintf(topic, MEASUREMENT_TOPIC_FMT, CentralduinoConfig.hub.device_id);

    StaticJsonDocument<MQTT_MAX_PACKET_SIZE> payload;
    payload[name] = value;

    char buffer[MQTT_MAX_PACKET_SIZE];
    serializeJson(payload, buffer);
    Log.trace("MQTT Publishing to: %s" CR, topic);
    Log.trace("Payload: %s" CR, buffer);
    _mqttClient.publish(topic, buffer);
}

void CentralduinoClass::registerDeviceMethod(const char *name, MethodCallbackFunctionType callback)
{
    // add the name & callback to our map
    methodRegistry[0].name = name;
    methodRegistry[0].callback = callback;
}

///////////////////////////////////////////////////////////////////
// Private helper methods

static void handleIncomingDirectMethod(char *methodName, byte *data, unsigned int length, char* rid)
{
    char responseTopic[100];
    // $iothub/methods/res/{status}/?$rid={request id}
    sprintf(responseTopic, "$iothub/methods/res/200/?$rid=%s", rid);

    Log.notice("Handling incoming direct method: %s (%s)" CR, methodName, rid);
    for (int i = 0; i < MAX_REGISTERED_METHODS; ++i)
    {
        if (strcmp(methodRegistry[i].name, methodName) == 0)
        {
            // Found it! Call it and bail.
            methodRegistry[i].callback();
            // TODO: Send back a response
            
            _mqttClient.publish(responseTopic, "{}");
            break;
        }
    }
}

static void handleIncomingMessage(char *topic, byte *data, unsigned int length)
{
    Log.trace("Incoming message received:" CR);
    Log.trace("- topic: %s" CR, topic);
    Log.trace("- data: %s" CR, data);

    // Process the topic string, tokenizing it by the /
    char *pch;
    pch = strtok(topic, "/");
    // Log.trace("pch=%s" CR, pch);
    if (pch == NULL)
    {
        Log.warning("Malformed topic string received (1): %s" CR, topic);
        return;
    }

    if (strcmp(pch, "$iothub") == 0)
    {
        // grab the next token to figure out the type
        pch = strtok(NULL, "/");
        if (strcmp(pch, "methods") == 0)
        {
            // It is a direct mehod, next token should be POST
            pch = strtok(NULL, "/");
            if (strcmp(pch, "POST") != 0)
            {
                Log.warning("Malformed topic string received (2): %s" CR, topic);
                return;
            }
            // followed by the method name
            pch = strtok(NULL, "/");
            char* rpch = strtok(NULL, "=");
            rpch = strtok(NULL, "=");
            Log.notice("Direct method received. Sending to handler." CR);
            handleIncomingDirectMethod(pch, data, length, rpch);
        }
    }
}

void CentralduinoClass::sendTwinUpdateRequest()
{
    const char *twin_topic = "$iothub/twin/GET/?$rid=0";
    if (!_mqttClient.publish(twin_topic, " "))
        Log.error("Failed to send Device Twin update request." CR);
    _mqttClient.loop();
}

void CentralduinoClass::registerCallbacks()
{
    StringBuffer buffer(64);
    buffer.setLength(snprintf(*buffer, 63, "devices/%s/messages/events/#", CentralduinoConfig.hub.device_id));

    int errorCode = 0;
    if ((errorCode = _mqttClient.subscribe(*buffer)) == 0)
        Log.error("ERROR: mqttClient couldn't subscribe to %s. error code => %d" CR, *buffer, errorCode);

    buffer.setLength(snprintf(*buffer, 63, "devices/%s/messages/devicebound/#", CentralduinoConfig.hub.device_id));

    if ((errorCode = _mqttClient.subscribe(*buffer)) == 0)
        Log.error("ERROR: mqttClient couldn't subscribe to %s. error code => %d" CR, *buffer, errorCode);

    errorCode = _mqttClient.subscribe("$iothub/twin/PATCH/properties/desired/#"); // twin desired property changes
    errorCode += _mqttClient.subscribe("$iothub/twin/res/#");                     // twin properties response
    errorCode += _mqttClient.subscribe("$iothub/methods/POST/#");                 // direct method calls

    if (errorCode < 3)
        Log.error("ERROR: mqttClient couldn't subscribe to twin/methods etc. error code sum => %d", errorCode);
}

void CentralduinoClass::ensureWiFiConnected()
{
    if (WiFi.status() == WL_CONNECTED)
        return;

    Log.notice("Connecting to WiFi." CR);
    WiFi.mode(WIFI_STA);

    // Dump available networks to terminal
    // WiFi.disconnect();
    // Log.notice("-------------------" CR);
    // Log.notice("Scan starting... ");
    // int numNets = WiFi.scanNetworks();
    // Log.notice("%d networks found" CR, numNets);
    // for (int i = 0; i < numNets; ++i)
    //     Log.notice("- %s" CR, WiFi.SSID(i).c_str());
    // Log.notice("-------------------" CR);
    // delay(1000);

    WiFi.begin(CentralduinoConfig.network.ssid, CentralduinoConfig.network.password);

    unsigned long startingMillis = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        // bail out after 30sec
        if ((millis() - startingMillis) > 30 * 1000)
        {
            Log.error("Unable to connect to WiFi. Restarting device.");
            ESP.restart();
            break; // shouldn't get here?
        }

        delay(1000);
    }
    Log.trace("WiFi connected successfully" CR);
}

void CentralduinoClass::ensureHubConnected()
{
    if (_mqttClient.connected())
        return;

    StringBuffer hostName, username, password;
    StringBuffer tmpHostname(128);

    Log.notice("Getting connection info");
    if (AzureDps.getHubHostName(DEFAULT_ENDPOINT, CentralduinoConfig.hub.scope_id, CentralduinoConfig.hub.device_id, CentralduinoConfig.hub.sas_key, *tmpHostname))
    {
        Log.error("Failed to get hub host from DPS. Unable to continue." CR);
        return;
    }

    // StringBuffer cstr(128);
    char cstr[CONN_STR_MAX_LEN];
    int rc = snprintf(cstr, CONN_STR_MAX_LEN,
                      "HostName=%s;DeviceId=%s;SharedAccessKey=%s", *tmpHostname, CentralduinoConfig.hub.device_id, CentralduinoConfig.hub.sas_key);
    assert(rc > 0 && rc < CONN_STR_MAX_LEN);

    // TODO: move into iotc_dps and do not re-parse from connection string
    StringBuffer deviceId;
    getUsernameAndPasswordFromConnectionString(cstr, rc, hostName, deviceId, username, password);

    Log.trace("** Generated MQTT connection strings **" CR);
    Log.trace("hostname: %s" CR, *hostName);
    Log.trace("deviceId: %s" CR, *deviceId);
    Log.trace("username: %s" CR, *username);
    Log.trace("password: %s" CR, *password);

    Log.notice("Setting up MQTT client..." CR);
    BearSSL::X509List certList(SSL_CA_PEM_DEF);
    _wifiClient.setX509Time(time(NULL));
    _wifiClient.setTrustAnchors(&certList);
    _mqttClient.setClient(_wifiClient);
    _mqttClient.setServer(*hostName, AZURE_MQTT_SERVER_PORT);
    _mqttClient.setCallback(handleIncomingMessage);

    this->_isHubConnected = false;
    // Loop until we're connected
    Log.trace("Attempting MQTT connection: %s, %s, %s" CR, *deviceId, *username, *password);
    while (!_mqttClient.connected())
    {
        if (_mqttClient.connect(*deviceId, *username, *password))
        {
            Log.trace("MQTT connected");
            break;
        }
        else
        {
            Log.error("MQTT connection failed, rc=%d. Will try again in 5 sec." CR, _mqttClient.state());
            delay(5000);
        }
    }
    this->_isHubConnected = true;
}

void CentralduinoClass::syncNtpTime()
{
    time_t epochTime;

    Log.notice("Configuring NTP..." CR);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    while (true)
    {
        epochTime = time(NULL);

        if (epochTime < MIN_EPOCH)
        {
            Log.warning("Fetching NTP epoch time failed! Waiting 2 seconds to retry." CR);
            delay(2000);
        }
        else
        {
            Log.notice("Fetched NTP epoch time is: %d" CR, epochTime);
            break;
        }

        delay(10);
    }

    struct timeval tv;
    tv.tv_sec = epochTime;
    tv.tv_usec = 0;

    settimeofday(&tv, NULL);
}

// JsonObject CentralduinoClass::getConfigJson()
// {
//     if (!SPIFFS.exists(CONFIG_FILE))
//     {
//         Log.error("Config file not found in SPIFFS! Unable to continue." CR);
//         delay(1000);
//         ESP.restart();
//     }

//     File file = SPIFFS.open(CONFIG_FILE, "r");
//     deserializeJson(this->_jsonDocument, file);
//     return this->_jsonDocument.as<JsonObject>();
// }

int CentralduinoClass::getUsernameAndPasswordFromConnectionString(const char *connectionString, size_t connectionStringLength,
                                                                  StringBuffer &hostName, StringBuffer &deviceId,
                                                                  StringBuffer &username, StringBuffer &password)
{
    // TODO: improve this so we don't depend on a particular order in connection string
    StringBuffer connStr(connectionString, connectionStringLength);

    int32_t hostIndex = connStr.indexOf(HOSTNAME_STRING, HOSTNAME_LENGTH);
    size_t length = connStr.getLength();
    if (hostIndex != 0)
    {
        Log.error("ERROR: connectionString doesn't start with HostName=  RESULT:%d", hostIndex);
        return 1;
    }

    int32_t deviceIndex = connStr.indexOf(DEVICEID_STRING, DEVICEID_LENGTH);
    if (deviceIndex == -1)
    {
        Log.error("ERROR: ;DeviceId= not found in the connectionString");
        return 1;
    }

    int32_t keyIndex = connStr.indexOf(KEY_STRING, KEY_LENGTH);
    if (keyIndex == -1)
    {
        Log.error("ERROR: ;SharedAccessKey= not found in the connectionString");
        return 1;
    }

    hostName.initialize(connectionString + HOSTNAME_LENGTH, deviceIndex - HOSTNAME_LENGTH);
    deviceId.initialize(connectionString + (deviceIndex + DEVICEID_LENGTH), keyIndex - (deviceIndex + DEVICEID_LENGTH));

    StringBuffer keyBuffer(length - (keyIndex + KEY_LENGTH));
    memcpy(*keyBuffer, connectionString + (keyIndex + KEY_LENGTH), keyBuffer.getLength());

    StringBuffer hostURLEncoded(hostName);
    hostURLEncoded.urlEncode();

    // size_t expires = time(NULL) + AUTH_EXPIRES;
    size_t expires = time(NULL) + AUTH_EXPIRES;
    Log.trace("Expires time is %ld" CR, expires);

    StringBuffer stringToSign(hostURLEncoded.getLength() + 128);
    StringBuffer deviceIdEncoded(deviceId);
    deviceIdEncoded.urlEncode();

    size_t keyLength = snprintf(*stringToSign, stringToSign.getLength(), "%s%s%s\n%zu000",
                                *hostURLEncoded, "%2Fdevices%2F", *deviceIdEncoded, expires);
    stringToSign.setLength(keyLength);

    keyBuffer.base64Decode();
    stringToSign.hash(*keyBuffer, keyBuffer.getLength());
    if (!stringToSign.base64Encode() || !stringToSign.urlEncode())
    {
        Log.error("ERROR: stringToSign base64Encode / urlEncode has failed.");
        return 1;
    }

    StringBuffer passwordBuffer(512);
    size_t passLength = snprintf(*passwordBuffer, 512,
                                 "SharedAccessSignature sr=%s%s%s&sig=%s&se=%zu000",
                                 *hostURLEncoded, "%2Fdevices%2F", *deviceIdEncoded, *stringToSign, expires);

    assert(passLength && passLength < 512);
    passwordBuffer.setLength(passLength);
    password.initialize(*passwordBuffer, passwordBuffer.getLength());

    const char *usernameTemplate = "%s/%s/api-version=2016-11-14";
    StringBuffer usernameBuffer((strlen(usernameTemplate) - 3 /* %s twice */) + hostName.getLength() + deviceId.getLength());

    size_t expLength = snprintf(*usernameBuffer, usernameBuffer.getLength(),
                                usernameTemplate, *hostName, *deviceId);
    assert(expLength <= usernameBuffer.getLength());

    username.initialize(*usernameBuffer, usernameBuffer.getLength());

    return 0;
}

///////////////////////////////////////////////////////////////////
// Allocate the global singleton declared in the .h file
CentralduinoClass Centralduino;
