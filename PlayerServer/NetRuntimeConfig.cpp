#include "NetRuntimeConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace {
std::string BufferToStdString(const Buffer& buf)
{
    return std::string(buf.data(), buf.data() + buf.size());
}

std::string Trim(const std::string& value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string ToUpper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

bool ParseBool(const std::string& value, bool fallback)
{
    const std::string upper = ToUpper(Trim(value));
    if (upper == "1" || upper == "TRUE" || upper == "YES" || upper == "ON") return true;
    if (upper == "0" || upper == "FALSE" || upper == "NO" || upper == "OFF") return false;
    return fallback;
}

unsigned int ParseUInt(const std::string& value, unsigned int fallback)
{
    try {
        size_t index = 0;
        const unsigned long parsed = std::stoul(Trim(value), &index, 0);
        if (index == 0) return fallback;
        return static_cast<unsigned int>(parsed);
    }
    catch (...) {
        return fallback;
    }
}

NetMode ParseMode(const std::string& value, NetMode fallback)
{
    const std::string upper = ToUpper(Trim(value));
    if (upper == "DPDKLEARN" || upper == "DPDK_LEARN" || upper == "DPDK") {
        return NetMode::DpdkLearn;
    }
    if (upper == "EPOLLONLY" || upper == "EPOLL_ONLY" || upper == "EPOLL") {
        return NetMode::EpollOnly;
    }
    return fallback;
}

void ApplyKV(NetRuntimeConfig& cfg, const std::string& key, const std::string& value)
{
    const std::string normalized = ToUpper(Trim(key));
    if (normalized == "MODE" || normalized == "NET_MODE") {
        cfg.mode = ParseMode(value, cfg.mode);
        return;
    }
    if (normalized == "DPDK_ENABLED") {
        cfg.dpdkEnabled = ParseBool(value, cfg.dpdkEnabled);
        return;
    }
    if (normalized == "DPDK_PORT_ID") {
        cfg.dpdkPortId = static_cast<unsigned short>(ParseUInt(value, cfg.dpdkPortId));
        return;
    }
    if (normalized == "DPDK_RX_QUEUES") {
        cfg.dpdkRxQueues = static_cast<unsigned short>(std::max(1u, ParseUInt(value, cfg.dpdkRxQueues)));
        return;
    }
    if (normalized == "DPDK_LCORE_MASK") {
        cfg.dpdkLcoreMask = Trim(value);
        return;
    }
    if (normalized == "DPDK_MBUF_COUNT") {
        cfg.dpdkMbufCount = std::max(1024u, ParseUInt(value, cfg.dpdkMbufCount));
        return;
    }
    if (normalized == "DPDK_BURST_SIZE") {
        cfg.dpdkBurstSize = static_cast<unsigned short>(std::max(1u, ParseUInt(value, cfg.dpdkBurstSize)));
        return;
    }
    if (normalized == "DPDK_LISTEN_UDP_PORT") {
        cfg.dpdkListenUdpPort = static_cast<unsigned short>(ParseUInt(value, cfg.dpdkListenUdpPort));
        return;
    }
}

void ApplyFileConfig(NetRuntimeConfig& cfg, const std::string& path)
{
    if (path.empty()) return;
    std::ifstream input(path.c_str());
    if (!input.is_open()) return;

    std::string line;
    while (std::getline(input, line)) {
        const size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);
        if (Trim(key).empty()) continue;
        ApplyKV(cfg, key, value);
    }
}

void ApplyEnvConfig(NetRuntimeConfig& cfg)
{
    struct EnvPair {
        const char* envName;
        const char* fieldName;
    };

    const EnvPair envs[] = {
        {"PLAYERSERVER_NET_MODE", "NET_MODE"},
        {"PLAYERSERVER_DPDK_ENABLED", "DPDK_ENABLED"},
        {"PLAYERSERVER_DPDK_PORT_ID", "DPDK_PORT_ID"},
        {"PLAYERSERVER_DPDK_RX_QUEUES", "DPDK_RX_QUEUES"},
        {"PLAYERSERVER_DPDK_LCORE_MASK", "DPDK_LCORE_MASK"},
        {"PLAYERSERVER_DPDK_MBUF_COUNT", "DPDK_MBUF_COUNT"},
        {"PLAYERSERVER_DPDK_BURST_SIZE", "DPDK_BURST_SIZE"},
        {"PLAYERSERVER_DPDK_LISTEN_UDP_PORT", "DPDK_LISTEN_UDP_PORT"},
    };

    for (const auto& item : envs) {
        const char* value = std::getenv(item.envName);
        if (!value || value[0] == '\0') continue;
        ApplyKV(cfg, item.fieldName, value);
    }
}
} // namespace

NetRuntimeConfig NetRuntimeConfig::Default()
{
    return NetRuntimeConfig();
}

NetRuntimeConfig NetRuntimeConfig::LoadFromEnvAndFile(const Buffer& explicitConfigFile)
{
    NetRuntimeConfig cfg = NetRuntimeConfig::Default();

    if (!explicitConfigFile.empty()) {
        cfg.configFilePath = explicitConfigFile;
    }
    else {
        const char* envPath = std::getenv("PLAYERSERVER_NET_CONFIG");
        if (envPath && envPath[0] != '\0') {
            cfg.configFilePath = envPath;
        }
    }

    ApplyFileConfig(cfg, BufferToStdString(cfg.configFilePath));
    ApplyEnvConfig(cfg);
    return cfg;
}

Buffer NetRuntimeConfig::ToString() const
{
    std::ostringstream oss;
    oss << "mode=" << (mode == NetMode::DpdkLearn ? "DpdkLearn" : "EpollOnly")
        << ", dpdkEnabled=" << (dpdkEnabled ? "true" : "false")
        << ", dpdkPortId=" << dpdkPortId
        << ", dpdkRxQueues=" << dpdkRxQueues
        << ", dpdkLcoreMask=" << (const char*)dpdkLcoreMask
        << ", dpdkMbufCount=" << dpdkMbufCount
        << ", dpdkBurstSize=" << dpdkBurstSize
        << ", dpdkListenUdpPort=" << dpdkListenUdpPort
        << ", configFilePath=" << (const char*)configFilePath;
    return oss.str();
}
