#include "DpdkRuntime.h"

#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#ifdef ENABLE_DPDK
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rte_byteorder.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_udp.h>
#endif

namespace {
const unsigned short kDefaultRxDesc = 1024;
const unsigned short kDefaultTxDesc = 1024;
const unsigned short kMaxBurst = 64;
} // namespace

DpdkRuntime::DpdkRuntime()
    : m_running(false)
    , m_dpdkReady(false)
    , m_rxPackets(0)
    , m_txPackets(0)
    , m_udpMatchedPackets(0)
    , m_droppedPackets(0)
#ifdef ENABLE_DPDK
    , m_pktMempool(nullptr)
#endif
{
}

DpdkRuntime::~DpdkRuntime()
{
    Stop();
}

int DpdkRuntime::Start(const NetRuntimeConfig& cfg)
{
    if (m_running.load()) return 0;
    m_cfg = cfg;

#ifndef ENABLE_DPDK
    TRACEW("DPDK is disabled at compile time. Rebuild with ENABLE_DPDK to enable DPDK mode.");
    return -100;
#else
    int ret = InitDpdk();
    if (ret != 0) return ret;

    ret = m_thread.SetThreadFunc(&DpdkRuntime::Loop, this);
    if (ret != 0) return -120;

    m_running.store(true);
    ret = m_thread.Start();
    if (ret != 0) {
        m_running.store(false);
        return -121;
    }
    TRACEI("DPDK runtime started. listen_udp_port=%u", m_cfg.dpdkListenUdpPort);
    return 0;
#endif
}

int DpdkRuntime::Stop()
{
    m_running.store(false);
    if (m_thread.isValid()) {
        m_thread.Stop();
    }

#ifdef ENABLE_DPDK
    if (m_dpdkReady.load()) {
        rte_eth_dev_stop(m_cfg.dpdkPortId);
        rte_eth_dev_close(m_cfg.dpdkPortId);
        m_dpdkReady.store(false);
    }
#endif
    return 0;
}

bool DpdkRuntime::IsRunning() const
{
    return m_running.load();
}

DpdkStats DpdkRuntime::Snapshot() const
{
    DpdkStats out;
    out.rxPackets = m_rxPackets.load();
    out.txPackets = m_txPackets.load();
    out.udpMatchedPackets = m_udpMatchedPackets.load();
    out.droppedPackets = m_droppedPackets.load();
    return out;
}

int DpdkRuntime::InitDpdk()
{
#ifndef ENABLE_DPDK
    return -100;
#else
    std::vector<std::string> args;
    args.push_back("playerserver_dpdk");
    args.push_back("-c");
    args.push_back((const char*)m_cfg.dpdkLcoreMask);
    args.push_back("-n");
    args.push_back("4");
    args.push_back("--proc-type=auto");

    std::vector<std::vector<char>> argStorage;
    argStorage.reserve(args.size());
    for (const auto& arg : args) {
        std::vector<char> one(arg.begin(), arg.end());
        one.push_back('\0');
        argStorage.push_back(one);
    }

    std::vector<char*> cargs;
    cargs.reserve(argStorage.size());
    for (auto& arg : argStorage) {
        cargs.push_back(arg.data());
    }

    const int ealRet = rte_eal_init(static_cast<int>(cargs.size()), cargs.data());
    if (ealRet < 0) {
        TRACEE("rte_eal_init failed.");
        return -101;
    }

    const uint16_t availablePorts = static_cast<uint16_t>(rte_eth_dev_count_avail());
    if (availablePorts == 0 || m_cfg.dpdkPortId >= availablePorts) {
        TRACEE("No valid DPDK port. available=%u request=%u", availablePorts, m_cfg.dpdkPortId);
        return -102;
    }

    m_pktMempool = rte_pktmbuf_pool_create(
        "playerserver_pkt_pool",
        m_cfg.dpdkMbufCount,
        256,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (m_pktMempool == nullptr) {
        TRACEE("rte_pktmbuf_pool_create failed.");
        return -103;
    }

    rte_eth_conf portConf;
    std::memset(&portConf, 0, sizeof(portConf));

    const uint16_t rxQueues = static_cast<uint16_t>(std::max(1, static_cast<int>(m_cfg.dpdkRxQueues)));
    int ret = rte_eth_dev_configure(m_cfg.dpdkPortId, rxQueues, 1, &portConf);
    if (ret < 0) {
        TRACEE("rte_eth_dev_configure failed, ret=%d", ret);
        return -104;
    }

    const int socketId = rte_eth_dev_socket_id(m_cfg.dpdkPortId);
    for (uint16_t queueId = 0; queueId < rxQueues; ++queueId) {
        ret = rte_eth_rx_queue_setup(
            m_cfg.dpdkPortId,
            queueId,
            kDefaultRxDesc,
            socketId,
            nullptr,
            m_pktMempool);
        if (ret < 0) {
            TRACEE("rte_eth_rx_queue_setup failed. queue=%u ret=%d", queueId, ret);
            return -105;
        }
    }

    ret = rte_eth_tx_queue_setup(m_cfg.dpdkPortId, 0, kDefaultTxDesc, socketId, nullptr);
    if (ret < 0) {
        TRACEE("rte_eth_tx_queue_setup failed, ret=%d", ret);
        return -106;
    }

    ret = rte_eth_dev_start(m_cfg.dpdkPortId);
    if (ret < 0) {
        TRACEE("rte_eth_dev_start failed, ret=%d", ret);
        return -107;
    }

    rte_eth_promiscuous_enable(m_cfg.dpdkPortId);
    rte_eth_stats_reset(m_cfg.dpdkPortId);
    m_dpdkReady.store(true);
    return 0;
#endif
}

int DpdkRuntime::Loop()
{
#ifndef ENABLE_DPDK
    return -100;
#else
    const uint16_t rxQueues = static_cast<uint16_t>(std::max(1, static_cast<int>(m_cfg.dpdkRxQueues)));
    const uint16_t burst = static_cast<uint16_t>(std::max(1, std::min(static_cast<int>(kMaxBurst), static_cast<int>(m_cfg.dpdkBurstSize))));
    std::time_t lastLogAt = std::time(nullptr);

    while (m_running.load()) {
        for (uint16_t queueId = 0; queueId < rxQueues; ++queueId) {
            rte_mbuf* mbufs[kMaxBurst];
            const uint16_t recv = rte_eth_rx_burst(m_cfg.dpdkPortId, queueId, mbufs, burst);
            if (recv == 0) continue;

            m_rxPackets.fetch_add(recv);
            for (uint16_t i = 0; i < recv; ++i) {
                rte_mbuf* mbuf = mbufs[i];
                const uint16_t pktLen = rte_pktmbuf_data_len(mbuf);

                if (pktLen < sizeof(rte_ether_hdr) + sizeof(rte_ipv4_hdr) + sizeof(rte_udp_hdr)) {
                    m_droppedPackets.fetch_add(1);
                    rte_pktmbuf_free(mbuf);
                    continue;
                }

                auto* eth = rte_pktmbuf_mtod(mbuf, rte_ether_hdr*);
                if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                    rte_pktmbuf_free(mbuf);
                    continue;
                }

                auto* ip = reinterpret_cast<rte_ipv4_hdr*>(eth + 1);
                if (ip->next_proto_id != IPPROTO_UDP) {
                    rte_pktmbuf_free(mbuf);
                    continue;
                }

                auto* udp = reinterpret_cast<rte_udp_hdr*>(
                    reinterpret_cast<uint8_t*>(ip) + ((ip->version_ihl & 0x0F) << 2));
                const uint16_t dstPort = rte_be_to_cpu_16(udp->dst_port);

                if (dstPort == m_cfg.dpdkListenUdpPort) {
                    m_udpMatchedPackets.fetch_add(1);

                    // Minimal echo for the learning path.
                    rte_ether_addr srcMac = eth->src_addr;
                    eth->src_addr = eth->dst_addr;
                    eth->dst_addr = srcMac;

                    const uint32_t srcIp = ip->src_addr;
                    ip->src_addr = ip->dst_addr;
                    ip->dst_addr = srcIp;
                    ip->hdr_checksum = 0;
                    ip->hdr_checksum = rte_ipv4_cksum(ip);

                    const uint16_t srcPort = udp->src_port;
                    udp->src_port = udp->dst_port;
                    udp->dst_port = srcPort;
                    udp->dgram_cksum = 0;

                    rte_mbuf* sendMbufs[1] = { mbuf };
                    const uint16_t sent = rte_eth_tx_burst(m_cfg.dpdkPortId, 0, sendMbufs, 1);
                    if (sent == 1) {
                        m_txPackets.fetch_add(1);
                        continue;
                    }
                    m_droppedPackets.fetch_add(1);
                    rte_pktmbuf_free(mbuf);
                    continue;
                }

                rte_pktmbuf_free(mbuf);
            }
        }

        const std::time_t now = std::time(nullptr);
        if (now - lastLogAt >= 1) {
            LogStats();
            lastLogAt = now;
        }
    }

    LogStats();
    return 0;
#endif
}

void DpdkRuntime::LogStats()
{
    const DpdkStats s = Snapshot();
    TRACEI(
        "DPDK stats: rx=%llu tx=%llu udpMatched=%llu dropped=%llu",
        static_cast<unsigned long long>(s.rxPackets),
        static_cast<unsigned long long>(s.txPackets),
        static_cast<unsigned long long>(s.udpMatchedPackets),
        static_cast<unsigned long long>(s.droppedPackets));
}
