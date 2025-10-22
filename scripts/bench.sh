#!/bin/bash

./build/example/bench \
    --backend=TCP \
    --side=Server \
    --where=Host \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=128 \
    --local_ip= \
    --local_port= \
    --remote_ip= \
    --remote_port= \
    --dev_pci_addr= \
    --rep_pci_addr= \
    --comch_name=

./build/example/bench \
    --backend=Comch \
    --side=Server \
    --where=DPU \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --dev_id=0000:03:00.1 \
    --rep_pci_addr=0000:43:00.1 \
    --comch_name=bench \
    --bulk_size=8

./build/example/bench \
    --backend=Comch \
    --side=Client \
    --where=Host \
    --rpc \
    --n_req=10 \
    --req_size=1024 \
    --n_fiber=4 \
    --bulk \
    --dev_id=0000:43:00.1 \
    --rep_pci_addr= \
    --comch_name=bench \
    --bulk_size=4

./build/example/bench \
    --backend=RDMA \
    --side=Server \
    --where=Host \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.200.20 \
    --local_port=10086 \
    --dev_pci_addr=0000:99:00.0

./build/example/bench \
    --backend=RDMA \
    --side=Client \
    --where=DPU \
    --rpc \
    --n_req=10 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.203.21 \
    --local_port=10087 \
    --remote_ip=192.168.203.21 \
    --remote_port=10086 \
    --dev_id=mlx5_3

./build/example/bench \
    --backend=RDMA \
    --side=Server \
    --where=Host \
    --rpc \
    --n_req=10 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.203.21 \
    --local_port=10086 \
    --dev_id=mlx5_3

./build/example/bench \
    --backend=RDMA \
    --side=Server \
    --where=Host \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.201.21 \
    --local_port=10086 \
    --dev_pci_addr=0000:43:00.1

./build/example/bench \
    --backend=RDMA \
    --side=Client \
    --where=Host \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.201.21 \
    --local_port=10087 \
    --remote_ip=192.168.201.20 \
    --remote_port=10086 \
    --dev_pci_addr=0000:43:00.1

./build/example/bench \
    --backend=RDMA \
    --side=Client \
    --where=Host \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.201.20 \
    --local_port=10087 \
    --remote_ip=192.168.201.21 \
    --remote_port=10086 \
    --dev_pci_addr=0000:99:00.1

./build/example/bench \
    --backend=RDMA \
    --side=Server \
    --where=DPU \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.203.20 \
    --local_port=10086 \
    --dev_pci_addr=0000:03:00.1

./build/example/bench \
    --backend=RDMA \
    --side=Client \
    --where=DPU \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.203.21 \
    --local_port=10087 \
    --remote_ip=192.168.203.20 \
    --remote_port=10086 \
    --dev_pci_addr=0000:03:00.1

./build/example/bench \
    --backend=RDMA \
    --side=Client \
    --where=DPU \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.203.20 \
    --local_port=10087 \
    --remote_ip=192.168.203.21 \
    --remote_port=10086 \
    --dev_pci_addr=mlx5_3

./build/example/bench \
    --backend=RDMA \
    --side=Server \
    --where=DPU \
    --rpc \
    --n_req=10000 \
    --req_size=4096 \
    --n_fiber=32 \
    --bulk \
    --bulk_size=32 \
    --local_ip=192.168.203.21 \
    --local_port=10086 \
    --dev_pci_addr=mlx5_3
