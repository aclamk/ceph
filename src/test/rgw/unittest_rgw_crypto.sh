#!/bin/bash
#
source $(dirname $0)/../detect-build-env-vars.sh
$CEPH_BIN/unittest_rgw_crypto --plugin_crypto_accelerator=none
$CEPH_BIN/unittest_rgw_crypto --plugin_crypto_accelerator=crypto_isal
$CEPH_BIN/unittest_rgw_crypto --plugin_crypto_accelerator=crypto_openssl
