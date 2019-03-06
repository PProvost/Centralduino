#ifndef __CONFIG_H
#define __CONFIG_H

#define NET_SSID_MAX_LEN    64
#define NET_PASS_MAX_LEN    64

#define HUB_SCOPE_MAX_LEN   128
#define HUB_DEVID_MAX_LEN   128
#define HUB_SASKEY_MAX_LEN  128

// TODO - Check if these string lengths are reasonable

typedef struct _NetworkConfigStruct
{
    char ssid[NET_SSID_MAX_LEN];
    char password[NET_PASS_MAX_LEN];
} _NetworkConfig;

typedef struct _HubConfigStruct
{
    char scope_id[HUB_SCOPE_MAX_LEN];
    char device_id[HUB_DEVID_MAX_LEN];
    char sas_key[HUB_SASKEY_MAX_LEN];
} _HubConfig;

class CentralduinoConfigClass
{
  public:
    _NetworkConfig network;
    _HubConfig hub;

    bool loadConfig(const char* path);
    void dumpConfigToLog();
};

// Declare the singleton
extern CentralduinoConfigClass CentralduinoConfig;

#endif