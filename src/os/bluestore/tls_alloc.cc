// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "tls_alloc.h"
#include "common/debug.h"
#include "common/admin_socket.h"
#define dout_subsys ceph_subsys_bluestore

using std::string;
using std::to_string;

using ceph::bufferlist;
using ceph::Formatter;


ssize_t onode_alloc::get_free() {
  std::lock_guard l(lock);
  ssize_t ret = size - alloc_pos + free_cnt;
  //temporary can go below 0
  return ret;
}

void* onode_alloc::allocate(size_t bytes, size_t alignment) {
  std::lock_guard l(lock);
  void *ptr;
  int rc = ::posix_memalign((void**)&ptr, alignment, bytes);
  if (rc)
    throw std::bad_alloc();
  alloc_pos += bytes;
  return ptr;
};

void onode_alloc::deallocate(void* p, size_t bytes, size_t alignment) {
  std::lock_guard l(lock);
  free_cnt += bytes;
  ::free(p);
};


//static onode_alloc temp_alloc;


class onode_alloc_mgr::SocketHook : public AdminSocketHook {
  onode_alloc_mgr *mgr;

  friend class Allocator;
  std::string name;
public:
  explicit SocketHook(onode_alloc_mgr *mgr) :
    mgr(mgr)
  {
    AdminSocket *admin_socket = mgr->cct->get_admin_socket();
    if (admin_socket) {
      int r = admin_socket->register_command(
	"bluestore onode_alloc_mgr",
	this,
	"dump onode_alloc_mgr status");
      if (r != 0)
        mgr = nullptr; //some collision, disable
    }
  }
  ~SocketHook()
  {
    AdminSocket *admin_socket = mgr->cct->get_admin_socket();
    if (admin_socket && mgr) {
      admin_socket->unregister_commands(this);
    }
  }

  int call(std::string_view command,
	   const cmdmap_t& cmdmap,
	   Formatter *f,
	   std::ostream& ss,
	   bufferlist& out) override {
    int r = 0;
    if (command == "bluestore onode_alloc_mgr") {
      f->open_array_section("regions");
      std::lock_guard l(mgr->lock);
      unsigned i = 0;
      for (auto& x: mgr->alloc_groups) {
	f->open_object_section("region");
	f->dump_unsigned("id", i);
	f->dump_format("storage", "%16.16lx", x->storage);
	f->dump_unsigned("alloc_pos", x->alloc_pos);
	f->dump_unsigned("free_cnt", x->free_cnt);
	f->close_section();
	i++;
      }
      f->close_section();
    } else {
      ss << "Invalid command" << std::endl;
      r = -ENOSYS;
    }
    return r;
  }
};




onode_alloc_mgr::onode_alloc_mgr(CephContext *cct)
  : cct(cct) {
  
  asok_hook = new onode_alloc_mgr::SocketHook(this);
};
onode_alloc_mgr::~onode_alloc_mgr() {
  delete asok_hook;
};


onode_alloc* onode_alloc_mgr::alloc_for_onode() {
  std::lock_guard l(lock);
  onode_alloc* alloc = nullptr;
  for (auto& x: alloc_groups) {
    if (x->get_free() > 0) {
      alloc = x;
      break;
    }
  }
  if (alloc == nullptr) {
    alloc = new onode_alloc(group_size);
    alloc_groups.push_back(alloc);
  }
  return alloc;
}



//devise algorithm for purging onode regions
