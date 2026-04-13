#!/usr/bin/env bash
set -euo pipefail

echo "[1/5] Check root privilege"
if [[ "${EUID}" -ne 0 ]]; then
  echo "Please run as root: sudo bash scripts/dpdk_dev_prepare.sh"
  exit 1
fi

echo "[2/5] Reserve hugepages"
mkdir -p /mnt/huge
if ! mountpoint -q /mnt/huge; then
  mount -t hugetlbfs nodev /mnt/huge
fi
sysctl -w vm.nr_hugepages=1024 >/dev/null

echo "[3/5] Load vfio/uio modules"
modprobe vfio-pci || true
modprobe uio || true
modprobe igb_uio || true

echo "[4/5] Show NIC status"
if command -v dpdk-devbind.py >/dev/null 2>&1; then
  dpdk-devbind.py -s
elif [[ -x /usr/share/dpdk/usertools/dpdk-devbind.py ]]; then
  /usr/share/dpdk/usertools/dpdk-devbind.py -s
else
  echo "dpdk-devbind.py not found. Install DPDK tools first."
fi

echo "[5/5] Next-step examples"
cat <<'EOF'
Bind NIC to vfio-pci (replace 0000:xx:yy.z):
  dpdk-devbind.py --bind=vfio-pci 0000:xx:yy.z

Restore NIC to kernel driver (replace ixgbe with your driver):
  dpdk-devbind.py --bind=ixgbe 0000:xx:yy.z
EOF

echo "DPDK dev environment prep completed."

