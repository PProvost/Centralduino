#include "azure_dps.h"

#include "defines.h"
#include "string_buffer.h"
// #include "ntphelper.h"

#include <ESP8266WiFi.h>
#include <ArduinoLog.h>
#include <assert.h>
#include <stddef.h>

int AzureDpsClass::getDPSAuthString(const char *scopeId, const char *deviceId, const char *key,
                     char *buffer, int bufferSize, size_t &outLength)
{
    size_t expires = time(NULL) + AUTH_EXPIRES;

    StringBuffer deviceIdEncoded(deviceId, strlen(deviceId));
    deviceIdEncoded.urlEncode();

    StringBuffer stringToSign(256);
    size_t size = snprintf(*stringToSign, 256, "%s%%2Fregistrations%%2F%s", scopeId, *deviceIdEncoded);
    assert(size < 256);
    stringToSign.setLength(size);

    StringBuffer sr(stringToSign);
    size = snprintf(*stringToSign, 256, "%s\n%lu000", *sr, expires);
    assert(size < 256);
    stringToSign.setLength(size);

    size = 0;
    StringBuffer keyDecoded(key, strlen(key));
    keyDecoded.base64Decode();
    stringToSign.hash(*keyDecoded, keyDecoded.getLength());
    if (!stringToSign.base64Encode() || !stringToSign.urlEncode())
    {
        Log.error("ERROR: stringToSign base64Encode / urlEncode has failed.");
        return 1;
    }

    outLength = snprintf(buffer, bufferSize, "authorization: SharedAccessSignature sr=%s&sig=%s&se=%lu000&skn=registration",
                         *sr, *stringToSign, expires);
    assert(outLength > 0 && outLength < bufferSize);
    buffer[outLength] = 0;

    return 0;
}

int AzureDpsClass::getOperationId(const char *dpsEndpoint, const char *scopeId, const char *deviceId,
                    const char *authHeader, char *operationId, char *hostName)
{
    // TODO: We can get rid of this and use the class member?
    WiFiClientSecure client;
    int exitCode = 0;

    BearSSL::X509List certList(SSL_CA_PEM_DEF);
    client.setX509Time(time(NULL));
    client.setTrustAnchors(&certList);

    int retry = 0;
    while (retry < 5 && !client.connect(dpsEndpoint, AZURE_HTTPS_SERVER_PORT))
        retry++;
    if (!client.connected())
    {
        Log.error("ERROR: DPS endpoint %s call has failed.", hostName == NULL ? "PUT" : "GET");
        return 1;
    }

    StringBuffer tmpBuffer(1024);
    StringBuffer deviceIdEncoded(deviceId, strlen(deviceId));
    deviceIdEncoded.urlEncode();

    size_t size = 0;
    if (hostName == NULL)
    {
        size = strlen("{\"registrationId\":\"%s\"}") + strlen(deviceId) - 2;
        size = snprintf(*tmpBuffer, 1024, "\
PUT /%s/registrations/%s/register?api-version=2018-11-01 HTTP/1.1\r\n\
Host: %s\r\n\
content-type: application/json; charset=utf-8\r\n\
%s\r\n\
accept: */*\r\n\
content-length: %d\r\n\
%s\r\n\
connection: close\r\n\
\r\n\
{\"registrationId\":\"%s\"}\r\n\
    ",
                        scopeId, *deviceIdEncoded, dpsEndpoint,
                        AZURE_IOT_CENTRAL_CLIENT_SIGNATURE, size, authHeader, deviceId);
    }
    else
    {

        size = snprintf(*tmpBuffer, 1024, "\
GET /%s/registrations/%s/operations/%s?api-version=2018-11-01 HTTP/1.1\r\n\
Host: %s\r\n\
content-type: application/json; charset=utf-8\r\n\
%s\r\n\
accept: */*\r\n\
%s\r\n\
connection: close\r\n\
\r\n",
                        scopeId, *deviceIdEncoded, operationId, dpsEndpoint,
                        AZURE_IOT_CENTRAL_CLIENT_SIGNATURE, authHeader);
    }

    assert(size != 0 && size < 1024);
    tmpBuffer.setLength(size);
    client.println(*tmpBuffer);
    int index = 0;
    while (!client.available())
    {
        delay(100);
        if (index++ > IOTC_SERVER_RESPONSE_TIMEOUT * 10)
        {
            // 20 secs..
            client.stop();
            Log.error("ERROR: DPS (%s) request has failed. (Server didn't answer within 20 secs.)", hostName == NULL ? "PUT" : "GET");
            return 1;
        }
    }

    index = 0;
    bool enableSaving = false;
    while (client.available() && index < 1024 - 1)
    {
        char ch = (char)client.read();
        if (ch == '{')
        {
            enableSaving = true; // don't use memory for headers
        }

        if (enableSaving)
        {
            (*tmpBuffer)[index++] = ch;
        }
    }
    tmpBuffer.setLength(index);

    const char *lookFor = hostName == NULL ? "{\"operationId\":\"" : "\"assignedHub\":\"";
    index = tmpBuffer.indexOf(lookFor, strlen(lookFor), 0);
    if (index == -1)
    {
    error_exit:
        Log.error("ERROR: DPS (%s) request has failed.\r\n%s", hostName == NULL ? "PUT" : "GET", *tmpBuffer);
        exitCode = 1;
        goto exit_operationId;
    }
    else
    {
        index += strlen(lookFor);
        int index2 = tmpBuffer.indexOf("\"", 1, index + 1);
        if (index2 == -1)
            goto error_exit;
        tmpBuffer.setLength(index2);
        strcpy(hostName == NULL ? operationId : hostName, (*tmpBuffer) + index);
    }

exit_operationId:
    client.stop();
    return exitCode;
}

int AzureDpsClass::getHubHostName(const char *dpsEndpoint, const char *scopeId, const char *deviceId, const char *key, char *hostName)
{
    StringBuffer authHeader(256);
    size_t size = 0;

    Log.trace("Getting auth string" CR);
    if (getDPSAuthString(scopeId, deviceId, key, *authHeader, 256, size))
    {
        Log.error("ERROR: getDPSAuthString has failed" CR);
        return 1;
    }
    Log.trace("Getting operation id for DPS" CR);
    StringBuffer operationId(64);
    int retval = 0;

    if ((retval = getOperationId(dpsEndpoint, scopeId, deviceId, *authHeader, *operationId, NULL)) == 0)
    {
        delay(250);
        Log.trace("Getting host name from DPS" CR);
        for (int i = 0; i < 5; i++)
        {
            // Log.trace("Stuff: %s : %s : %s : %s : %s : %s" CR, dpsEndpoint, scopeId, deviceId, *authHeader, *operationId, hostName);
            retval = getOperationId(dpsEndpoint, scopeId, deviceId, *authHeader, *operationId, hostName);
            if (retval == 0)
                break;
            delay(250);
        }
    }

    return retval;
}

///////////////////////////////////////////////////////////////////
// Allocate the global singleton declared in the .h file
AzureDpsClass AzureDps;
