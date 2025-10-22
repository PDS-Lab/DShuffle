#!/bin/bash

# must mount hugetlbfs at custom mount point for user

# sudo dpdk-hugepages.py -U lsc -d "$(readlink -f ./.hugepage-2MB)" -m

# sudo mount -t hugetlbfs nodev .hugepage-2MB

# remember to chown or chmod /dev/vfio/xx

# tmux new-session -d -s tgt ""

LD_LIBRARY_PATH=/home/lsc/dpx/subprojects/spdk/build/lib:/home/lsc/dpx/subprojects/spdk/dpdk/build/lib \
    ./subprojects/spdk/build/bin/spdk_tgt -m [32] --huge-dir="$(readlink -f ./.hugepage-2MB)" -r "$(readlink -f ./.tgt/tgt.sock)"
