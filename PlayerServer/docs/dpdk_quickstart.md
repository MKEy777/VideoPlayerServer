# DPDK 快速上手（新手路径）

本项目会继续保留原有 `19527` 的 HTTP/TCP 主链路，同时新增可选的 DPDK UDP 学习链路。

## 1. 运行模式

- 默认模式（`EpollOnly`）：不依赖 DPDK，行为与当前版本一致。
- 学习模式（`DpdkLearn`）：启用 DPDK 运行时处理 UDP 报文（默认 UDP 端口 `19528`）。

## 2. 编译

### 不启用 DPDK（默认）

按当前方式正常编译即可。

### 启用 DPDK

1. 在 Linux 环境安装 DPDK 23.11 LTS。
2. 增加预处理宏 `ENABLE_DPDK`。
3. 在构建系统中链接 DPDK 相关库。

如果未启用 `ENABLE_DPDK`，程序在运行时会自动回退到 `EpollOnly`，不会影响主业务端口。

## 3. 运行配置优先级

1. 环境变量（最高优先级）
2. `./net_runtime.conf`（次优先级）
3. 代码默认值（兜底）

## 4. 最小配置示例

创建或修改 `net_runtime.conf`：

```ini
NET_MODE=DpdkLearn
DPDK_ENABLED=true
DPDK_PORT_ID=0
DPDK_RX_QUEUES=1
DPDK_LCORE_MASK=0x3
DPDK_MBUF_COUNT=8192
DPDK_BURST_SIZE=32
DPDK_LISTEN_UDP_PORT=19528
```

也可以使用环境变量覆盖：

```bash
export PLAYERSERVER_NET_MODE=DpdkLearn
export PLAYERSERVER_DPDK_ENABLED=true
export PLAYERSERVER_DPDK_PORT_ID=0
```

## 5. 验证步骤

1. 启动服务。
2. 验证原有主链路是否正常：
   - `curl http://<host>:19527/login?...`
3. 向 DPDK 监听端口发送 UDP 数据：
   - `nc -u <host> 19528`
4. 查看日志统计：
   - `DPDK stats: rx=... tx=... udpMatched=... dropped=...`

## 6. 说明

- 目的端口为 `19528` 的 UDP 报文会被统计，并执行最小回显（echo）。
- DPDK 初始化失败时会自动降级为 `EpollOnly`，主链路仍保持可用。

