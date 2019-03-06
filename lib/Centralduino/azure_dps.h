#ifndef __AZURE_DPS_H
#define __AZURE_DPS_H

#include <stddef.h>

class AzureDpsClass
{
  public:
    int getDPSAuthString(const char *scopeId, const char *deviceId, const char *key,
                         char *buffer, int bufferSize, size_t &outLength);
    int getOperationId(const char *dpsEndpoint, const char *scopeId, const char *deviceId,
                        const char *authHeader, char *operationId, char *hostName);
    int getHubHostName(const char *dpsEndpoint, const char *scopeId, const char *deviceId, const char *key, char *hostName);
};

extern AzureDpsClass AzureDps;

#endif