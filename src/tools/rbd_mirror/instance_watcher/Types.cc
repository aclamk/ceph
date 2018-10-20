// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "Types.h"
#include "include/ceph_assert.h"
#include "include/stringify.h"
#include "common/Formatter.h"

namespace rbd {
namespace mirror {
namespace instance_watcher {

namespace {

class EncodePayloadVisitor : public boost::static_visitor<void> {
public:
  explicit EncodePayloadVisitor(bufferlist &bl) : m_bl(bl) {}

  template <typename Payload>
  inline void operator()(const Payload &payload) const {
    using ceph::encode;
    encode(static_cast<uint32_t>(Payload::NOTIFY_OP), m_bl);
    payload.encode(m_bl);
  }

private:
  bufferlist &m_bl;
};

class DecodePayloadVisitor : public boost::static_visitor<void> {
public:
  DecodePayloadVisitor(__u8 version, bufferlist::const_iterator &iter)
    : m_version(version), m_iter(iter) {}

  template <typename Payload>
  inline void operator()(Payload &payload) const {
    payload.decode(m_version, m_iter);
  }

private:
  __u8 m_version;
  bufferlist::const_iterator &m_iter;
};

class DumpPayloadVisitor : public boost::static_visitor<void> {
public:
  explicit DumpPayloadVisitor(Formatter *formatter) : m_formatter(formatter) {}

  template <typename Payload>
  inline void operator()(const Payload &payload) const {
    NotifyOp notify_op = Payload::NOTIFY_OP;
    m_formatter->dump_string("notify_op", stringify(notify_op));
    payload.dump(m_formatter);
  }

private:
  ceph::Formatter *m_formatter;
};

} // anonymous namespace

template <class TT> void PayloadBase::encode(TT &bl) const {
  using ceph::encode;
  encode(request_id, bl);
}
template void PayloadBase::encode<bufferlist>(bufferlist &bl) const;
template void PayloadBase::encode<encode_size>(encode_size &bl) const;
template void PayloadBase::encode<encode_helper>(encode_helper &bl) const;

void PayloadBase::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(request_id, iter);
}

void PayloadBase::dump(Formatter *f) const {
  f->dump_unsigned("request_id", request_id);
}

template <class TT> void ImagePayloadBase::encode(TT &bl) const {
  using ceph::encode;
  PayloadBase::encode(bl);
  encode(global_image_id, bl);
}
template void ImagePayloadBase::encode<bufferlist>(bufferlist &bl) const;
template void ImagePayloadBase::encode<encode_size>(encode_size &bl) const;
template void ImagePayloadBase::encode<encode_helper>(encode_helper &bl) const;

void ImagePayloadBase::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  PayloadBase::decode(version, iter);
  decode(global_image_id, iter);
}

void ImagePayloadBase::dump(Formatter *f) const {
  PayloadBase::dump(f);
  f->dump_string("global_image_id", global_image_id);
}

template <class TT> void PeerImageRemovedPayload::encode(TT &bl) const {
  using ceph::encode;
  PayloadBase::encode(bl);
  encode(global_image_id, bl);
  encode(peer_mirror_uuid, bl);
}
template void PeerImageRemovedPayload::encode<bufferlist>(bufferlist &bl) const;
template void PeerImageRemovedPayload::encode<encode_size>(encode_size &bl) const;
template void PeerImageRemovedPayload::encode<encode_helper>(encode_helper &bl) const;

void PeerImageRemovedPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  PayloadBase::decode(version, iter);
  decode(global_image_id, iter);
  decode(peer_mirror_uuid, iter);
}

void PeerImageRemovedPayload::dump(Formatter *f) const {
  PayloadBase::dump(f);
  f->dump_string("global_image_id", global_image_id);
  f->dump_string("peer_mirror_uuid", peer_mirror_uuid);
}

template <class TT> void SyncPayloadBase::encode(TT &bl) const {
  using ceph::encode;
  PayloadBase::encode(bl);
  encode(sync_id, bl);
}
template void SyncPayloadBase::encode<bufferlist>(bufferlist &bl) const;
template void SyncPayloadBase::encode<encode_size>(encode_size &bl) const;
template void SyncPayloadBase::encode<encode_helper>(encode_helper &bl) const;

void SyncPayloadBase::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  PayloadBase::decode(version, iter);
  decode(sync_id, iter);
}

void SyncPayloadBase::dump(Formatter *f) const {
  PayloadBase::dump(f);
  f->dump_string("sync_id", sync_id);
}

template <class TT> void UnknownPayload::encode(TT &bl) const {
  ceph_abort();
}
template void UnknownPayload::encode<bufferlist>(bufferlist &bl) const;
template void UnknownPayload::encode<encode_size>(encode_size &bl) const;
template void UnknownPayload::encode<encode_helper>(encode_helper &bl) const;

void UnknownPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
}

void UnknownPayload::dump(Formatter *f) const {
}

template <class TT> void NotifyMessage::encode(TT& bl) const {
  ENCODE_START(2, 2, bl);
  boost::apply_visitor(EncodePayloadVisitor(bl), payload);
  ENCODE_FINISH(bl);
}
template void NotifyMessage::encode<bufferlist>(bufferlist& bl) const;
template void NotifyMessage::encode<encode_size>(encode_size& bl) const;
template void NotifyMessage::encode<encode_helper>(encode_helper& bl) const;

void NotifyMessage::decode(bufferlist::const_iterator& iter) {
  DECODE_START(2, iter);

  uint32_t notify_op;
  decode(notify_op, iter);

  // select the correct payload variant based upon the encoded op
  switch (notify_op) {
  case NOTIFY_OP_IMAGE_ACQUIRE:
    payload = ImageAcquirePayload();
    break;
  case NOTIFY_OP_IMAGE_RELEASE:
    payload = ImageReleasePayload();
    break;
  case NOTIFY_OP_PEER_IMAGE_REMOVED:
    payload = PeerImageRemovedPayload();
    break;
  case NOTIFY_OP_SYNC_REQUEST:
    payload = SyncRequestPayload();
    break;
  case NOTIFY_OP_SYNC_START:
    payload = SyncStartPayload();
    break;
  default:
    payload = UnknownPayload();
    break;
  }

  apply_visitor(DecodePayloadVisitor(struct_v, iter), payload);
  DECODE_FINISH(iter);
}

void NotifyMessage::dump(Formatter *f) const {
  apply_visitor(DumpPayloadVisitor(f), payload);
}

void NotifyMessage::generate_test_instances(std::list<NotifyMessage *> &o) {
  o.push_back(new NotifyMessage(ImageAcquirePayload()));
  o.push_back(new NotifyMessage(ImageAcquirePayload(1, "gid")));

  o.push_back(new NotifyMessage(ImageReleasePayload()));
  o.push_back(new NotifyMessage(ImageReleasePayload(1, "gid")));

  o.push_back(new NotifyMessage(PeerImageRemovedPayload()));
  o.push_back(new NotifyMessage(PeerImageRemovedPayload(1, "gid", "uuid")));

  o.push_back(new NotifyMessage(SyncRequestPayload()));
  o.push_back(new NotifyMessage(SyncRequestPayload(1, "sync_id")));

  o.push_back(new NotifyMessage(SyncStartPayload()));
  o.push_back(new NotifyMessage(SyncStartPayload(1, "sync_id")));
}

std::ostream &operator<<(std::ostream &out, const NotifyOp &op) {
  switch (op) {
  case NOTIFY_OP_IMAGE_ACQUIRE:
    out << "ImageAcquire";
    break;
  case NOTIFY_OP_IMAGE_RELEASE:
    out << "ImageRelease";
    break;
  case NOTIFY_OP_PEER_IMAGE_REMOVED:
    out << "PeerImageRemoved";
    break;
  case NOTIFY_OP_SYNC_REQUEST:
    out << "SyncRequest";
    break;
  case NOTIFY_OP_SYNC_START:
    out << "SyncStart";
    break;
  default:
    out << "Unknown (" << static_cast<uint32_t>(op) << ")";
    break;
  }
  return out;
}

template <class TT> void NotifyAckPayload::encode(TT &bl) const {
  using ceph::encode;
  encode(instance_id, bl);
  encode(request_id, bl);
  encode(ret_val, bl);
}
template void NotifyAckPayload::encode<bufferlist>(bufferlist &bl) const;
template void NotifyAckPayload::encode<encode_size>(encode_size &bl) const;
template void NotifyAckPayload::encode<encode_helper>(encode_helper &bl) const;

void NotifyAckPayload::decode(bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(instance_id, iter);
  decode(request_id, iter);
  decode(ret_val, iter);
}

void NotifyAckPayload::dump(Formatter *f) const {
  f->dump_string("instance_id", instance_id);
  f->dump_unsigned("request_id", request_id);
  f->dump_int("request_id", ret_val);
}

} // namespace instance_watcher
} // namespace mirror
} // namespace rbd
