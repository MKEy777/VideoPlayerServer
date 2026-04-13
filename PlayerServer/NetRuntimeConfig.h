#pragma once

#include "Public.h"

enum class NetMode {
    EpollOnly = 0,
    DpdkLearn = 1
};

struct NetRuntimeConfig {
    NetMode mode = NetMode::EpollOnly;
    bool dpdkEnabled = false;
    unsigned short dpdkPortId = 0;
    unsigned short dpdkRxQueues = 1;
    Buffer dpdkLcoreMask = "0x3";
    unsigned int dpdkMbufCount = 8192;
    unsigned short dpdkBurstSize = 32;
    unsigned short dpdkListenUdpPort = 19528;
    Buffer configFilePath = "./net_runtime.conf";

    static NetRuntimeConfig Default();
    static NetRuntimeConfig LoadFromEnvAndFile(const Buffer& explicitConfigFile = "");
    Buffer ToString() const;
};

