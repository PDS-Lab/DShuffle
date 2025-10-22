#!/bin/bash

java -XX:+UnlockDiagnosticVMOptions -XX:+PrintCompressedOopsMode -XX:MaxMetaspaceSize=2g -XX:MetaspaceSize=2g -XX:CompressedClassSpaceSize=1g -Xms14g -Xmx14g -XX:HeapBaseMinAddress=2g -jar /home/lsc/dpx/target/dpx-1.0-SNAPSHOT-jar-with-dependencies.jar  2>trace.log 1>dump.log
# java -XX:+UnlockDiagnosticVMOptions -XX:+PrintCompressedOopsMode -XX:MaxMetaspaceSize=2g -XX:MetaspaceSize=2g -XX:CompressedClassSpaceSize=1g -Xms14g -Xmx14g -XX:HeapBaseMinAddress=2g -cp /home/lsc/dpx/target/dpx-1.0-SNAPSHOT-jar-with-dependencies.jar pdsl.dpx.env.EnvParamLoader