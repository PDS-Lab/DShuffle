#!/bin/bash

set -x

setup() {

sock_path=$(readlink -f .tgt/tgt.sock)
subprojects/spdk/scripts/rpc.py -s "$sock_path" nvmf_create_transport -t RDMA -u 8192 -i 131072 -c 4096 -q 1024
# subprojects/spdk/scripts/rpc.py -s "$sock_path" bdev_nvme_attach_controller -b NVMe1 -t PCIe -a 0000:71:00.0
# subprojects/spdk/scripts/rpc.py -s "$sock_path" bdev_nvme_attach_controller -b NVMe1 -t PCIe -a 0000:3f:00.0
subprojects/spdk/scripts/rpc.py -s "$sock_path" bdev_nvme_attach_controller -b NVMe1 -t PCIe -a "$1"
subprojects/spdk/scripts/rpc.py -s "$sock_path" nvmf_create_subsystem nqn.2016-06.io.spdk:cnode1 -a -s SPDK00000000000001 -d SPDK_Controller1
subprojects/spdk/scripts/rpc.py -s "$sock_path" nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 NVMe1n1
# subprojects/spdk/scripts/rpc.py -s "$sock_path" nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode1 -t rdma -a 192.168.200.20 -s 4420
subprojects/spdk/scripts/rpc.py -s "$sock_path" nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode1 -t rdma -a "$2" -s 4420

}

# sudo nvme discover -t rdma -a 192.168.200.21 -s 4420
# sudo nvme connect -t rdma -n "nqn.2016-06.io.spdk:cnode1" -a 192.168.200.21 -s 4420

# sudo nvme discover -t rdma -a 192.168.200.20 -s 4420
# sudo nvme connect -t rdma -n "nqn.2016-06.io.spdk:cnode1" -a 192.168.200.20 -s 4420

# s20 sudo DEV_TYPE=NVME PCI_ALLOWED=0000:3f:00.0 ./subprojects/spdk/scripts/setup.sh
# s21 sudo DEV_TYPE=NVME PCI_ALLOWED=0000:71:00.0 ./subprojects/spdk/scripts/setup.sh

# sudo nvme disconnect -n nqn.2016-06.io.spdk:cnode1


case $1 in
s20)
    setup 0000:3f:00.0 192.168.200.20
    ;;
s21)
    setup 0000:71:00.0 192.168.200.21
    ;;
*)
    echo "Usage: $0 [s20|s21]"
    exit 1
    ;;
esac
