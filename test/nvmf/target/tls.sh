#!/usr/bin/env bash

testdir=$(readlink -f $(dirname $0))
rootdir=$(readlink -f $testdir/../../..)

source $rootdir/test/common/autotest_common.sh
source $rootdir/test/nvmf/common.sh

rpc_py="$rootdir/scripts/rpc.py"

timing_enter nvmf_ssl_test
nvmftestinit
nvmfappstart --wait-for-rpc

if [ "$TEST_TRANSPORT" != tcp ]; then
	echo "Unsupported transport: $TEST_TRANSPORT"
	exit 0
fi

# TODO: remove
NVMF_FIRST_TARGET_IP=127.0.0.1

$rpc_py sock_set_default_impl -i ssl
$rpc_py framework_start_init
$rpc_py nvmf_create_transport $NVMF_TRANSPORT_OPTS
$rpc_py nvmf_create_subsystem nqn.2016-06.io.spdk:cnode1 -a -s SPDK00000000000001 -m 10
$rpc_py nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode1 -t $TEST_TRANSPORT \
	-a $NVMF_FIRST_TARGET_IP -s $NVMF_PORT
$rpc_py bdev_malloc_create 32 4096 -b malloc0
$rpc_py nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 malloc0 -n 1

# Send IO
"${NVMF_TARGET_NS_CMD[@]}" $SPDK_EXAMPLE_DIR/perf -S ssl -q 64 -o 4096 -w randrw -M 30 -t 10 \
	-r "trtype:${TEST_TRANSPORT} adrfam:IPv4 traddr:${NVMF_FIRST_TARGET_IP} trsvcid:${NVMF_PORT} \
subnqn:nqn.2016-06.io.spdk:cnode1"

trap - SIGINT SIGTERM EXIT
nvmftestfini
timing_exit nvmf_ssl_test
