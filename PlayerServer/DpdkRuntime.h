#pragma once

#include "NetRuntimeConfig.h"
#include "Thread.h"

#include <atomic>
#include <cstdint>

struct rte_mempool;

struct DpdkStats {
    uint64_t rxPackets = 0;
    uint64_t txPackets = 0;
    uint64_t udpMatchedPackets = 0;
    uint64_t droppedPackets = 0;
};

class DpdkRuntime {
public:
    DpdkRuntime();
    ~DpdkRuntime();

    DpdkRuntime(const DpdkRuntime&) = delete;
    DpdkRuntime& operator=(const DpdkRuntime&) = delete;

public:
    int Start(const NetRuntimeConfig& cfg);
    int Stop();
    bool IsRunning() const;
    DpdkStats Snapshot() const;

private:
    int InitDpdk();
    int Loop();
    void LogStats();

private:
    NetRuntimeConfig m_cfg;
    CThread m_thread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_dpdkReady;
    std::atomic<uint64_t> m_rxPackets;
    std::atomic<uint64_t> m_txPackets;
    std::atomic<uint64_t> m_udpMatchedPackets;
    std::atomic<uint64_t> m_droppedPackets;

#ifdef ENABLE_DPDK
    struct rte_mempool* m_pktMempool;
#endif
};
