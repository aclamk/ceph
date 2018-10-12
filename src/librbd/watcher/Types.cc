// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "librbd/watcher/Types.h"
#include "common/Formatter.h"

namespace librbd {
namespace watcher {

template <class TT> void ClientId::encode(TT &bl) const {
  using ceph::encode;
  encode(gid, bl);
  encode(handle, bl);
}
template void ClientId::encode<bufferlist&>(bufferlist &bl) const;
template void ClientId::encode<encode_size&>(encode_size &bl) const;
template void ClientId::encode<encode_helper&>(encode_helper &bl) const;

void ClientId::decode(bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(gid, iter);
  decode(handle, iter);
}

void ClientId::dump(Formatter *f) const {
  f->dump_unsigned("gid", gid);
  f->dump_unsigned("handle", handle);
}

template <class TT> void NotifyResponse::encode(TT& bl) const {
  using ceph::encode;
  encode(acks, bl);
  encode(timeouts, bl);
}
template void NotifyResponse::encode<bufferlist&>(bufferlist& bl) const;
template void NotifyResponse::encode<encode_size&>(encode_size& bl) const;
template void NotifyResponse::encode<encode_helper&>(encode_helper& bl) const;

void NotifyResponse::decode(bufferlist::const_iterator& iter) {
  using ceph::decode;
  decode(acks, iter);
  decode(timeouts, iter);
}
std::ostream &operator<<(std::ostream &out,
                         const ClientId &client_id) {
  out << "[" << client_id.gid << "," << client_id.handle << "]";
  return out;
}

} // namespace watcher
} // namespace librbd
