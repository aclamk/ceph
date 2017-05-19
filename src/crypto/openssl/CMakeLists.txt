## openssl

set(openssl_crypto_plugin_srcs
  openssl_crypto_accel.cc 
  openssl_crypto_plugin.cc)

add_library(ceph_crypto_openssl SHARED ${openssl_crypto_plugin_srcs})
add_dependencies(ceph_crypto_openssl ${CMAKE_SOURCE_DIR}/src/ceph_ver.h)
set_target_properties(ceph_crypto_openssl PROPERTIES VERSION 1.0.0 SOVERSION 1)
install(TARGETS ceph_crypto_openssl DESTINATION ${crypto_plugin_dir})
