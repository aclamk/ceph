// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "tls_alloc.h"
#include "common/debug.h"
#define dout_subsys ceph_subsys_bluestore

using std::string;
using std::to_string;

using ceph::bufferlist;
using ceph::Formatter;


void* onode_alloc::allocate(size_t bytes, size_t alignment) {
  void *ptr;
  int rc = ::posix_memalign((void**)&ptr, alignment, bytes);
  if (rc)
    throw std::bad_alloc();
  return ptr;
};

void onode_alloc::deallocate(void* p, size_t bytes, size_t alignment) {
  ::free(p);
};


static onode_alloc temp_alloc;

onode_alloc* onode_alloc_mgr::alloc_for_onode() {
  return &temp_alloc;
  //search enough emoty onode_alloc (always last one?)
  
}

//devise algorithm for purging onode regions
