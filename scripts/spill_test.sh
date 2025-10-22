#!/bin/bash

case $1 in
s20)
    LD_LIBRARY_PATH=/home/lsc/dpx/src/main/resources/native/lib java -XX:+UnlockDiagnosticVMOptions -XX:+PrintCompressedOopsMode -XX:MaxMetaspaceSize=2g -XX:MetaspaceSize=2g -XX:CompressedClassSpaceSize=1g -Xms14g -Xmx14g -XX:HeapBaseMinAddress=2g -cp /home/lsc/dpx/target/dpx-1.0-SNAPSHOT-jar-with-dependencies.jar pdsl.dpx.bench.Spill "0000:99:00.1" "/home/lsc/dpx/.test_spill" "/dev/nvme3n1p1" 16 10000000 1024 1024 2>trace.log 1>dump.log
    ;;
s21)
    LD_LIBRARY_PATH=/home/lsc/dpx/src/main/resources/native/lib java -XX:+UnlockDiagnosticVMOptions -XX:+PrintCompressedOopsMode -XX:MaxMetaspaceSize=2g -XX:MetaspaceSize=2g -XX:CompressedClassSpaceSize=1g -Xms14g -Xmx14g -XX:HeapBaseMinAddress=2g -cp /home/lsc/dpx/target/dpx-1.0-SNAPSHOT-jar-with-dependencies.jar pdsl.dpx.bench.Spill "0000:43:00.1" "/home/lsc/dpx/.test_spill" "/dev/nvme2n1p1" 16 10000000 1024 1024 2>trace.log 1>dump.log
    ;;
*)
    echo "Usage: $0 [s20|s21]"
    exit 1
    ;;
esac
