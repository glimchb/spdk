#!/usr/bin/env bash

testdir=$(readlink -f $(dirname $0))
rootdir=$(readlink -f $testdir/../../..)

source $rootdir/test/common/autotest_common.sh
source $rootdir/test/nvmf/common.sh

rpc_py="$rootdir/scripts/rpc.py"

nvmftestinit
nvmfappstart -m 0x2 --wait-for-rpc

if [ "$TEST_TRANSPORT" != tcp ]; then
	echo "Unsupported transport: $TEST_TRANSPORT"
	exit 0
fi

$rpc_py sock_set_default_impl -i ssl
$rpc_py sock_impl_get_options -i ssl
$rpc_py sock_impl_set_options -i ssl --psk-key 1234567890ABCDEF
psk=$($rpc_py sock_impl_get_options -i ssl | jq -r .psk_key)
if [[ "$psk" != "1234567890ABCDEF" ]]; then
	echo "PSK KEY was not set correctly $psk != 1234567890ABCDEF"
	exit 1
fi
$rpc_py sock_impl_set_options -i ssl --psk-identity nqn.2014-08.org.nvmexpress:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6
psk=$($rpc_py sock_impl_get_options -i ssl | jq -r .psk_identity)
if [[ "$psk" != "nqn.2014-08.org.nvmexpress:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6" ]]; then
	echo "PSK ID was not set correctly $psk != nqn.2014-08.org.nvmexpress:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6"
	exit 1
fi

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
