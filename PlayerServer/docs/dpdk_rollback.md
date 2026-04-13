# DPDK 回滚指南

当你希望恢复为“仅内核网络栈”运行时，按下面步骤执行。

## 1. 关闭 DPDK 运行时

二选一即可：

- 使用环境变量：
  - `export PLAYERSERVER_DPDK_ENABLED=false`
  - `export PLAYERSERVER_NET_MODE=EpollOnly`
- 或修改 `net_runtime.conf`：

```ini
NET_MODE=EpollOnly
DPDK_ENABLED=false
```

修改后重启服务使配置生效。

## 2. 恢复网卡到内核驱动

如果网卡之前绑定到 `vfio-pci` 或 `igb_uio`，请绑定回内核驱动：

```bash
dpdk-devbind.py --bind=<kernel_driver> 0000:xx:yy.z
```

常见内核驱动示例：`ixgbe`、`i40e`、`mlx5_core`。

## 3. 释放 HugePages（可选）

```bash
sudo sysctl -w vm.nr_hugepages=0
sudo umount /mnt/huge || true
```

## 4. 回滚验证

1. 启动服务。
2. 验证 `19527` 的 HTTP 主链路可正常访问。
3. 确认日志中不再出现 DPDK 运行循环统计。

