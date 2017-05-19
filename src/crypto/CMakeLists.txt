## crypto plugins

set(crypto_plugin_dir ${CMAKE_INSTALL_PKGLIBDIR}/crypto)

add_subdirectory(openssl)
if(HAVE_INTEL)
  add_subdirectory(isa-l)
endif(HAVE_INTEL)

add_custom_target(crypto_plugins DEPENDS ceph_crypto_openssl)
if(HAVE_GOOD_YASM_ELF64)
add_dependencies(crypto_plugins ceph_crypto_isal)
endif(HAVE_GOOD_YASM_ELF64)
