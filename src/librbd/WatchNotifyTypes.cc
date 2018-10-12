// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "cls/rbd/cls_rbd_types.h"
#include "common/Formatter.h"
#include "include/ceph_assert.h"
#include "include/stringify.h"
#include "librbd/WatchNotifyTypes.h"
#include "librbd/watcher/Utils.h"

namespace librbd {
namespace watch_notify {

namespace {

class CheckForRefreshVisitor  : public boost::static_visitor<bool> {
public:
  template <typename Payload>
  inline bool operator()(const Payload &payload) const {
    return Payload::CHECK_FOR_REFRESH;
  }
};

class GetNotifyOpVisitor  : public boost::static_visitor<NotifyOp> {
public:
  template <typename Payload>
  NotifyOp operator()(const Payload &payload) const {
    return Payload::NOTIFY_OP;
  }
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

template <class TT> void AsyncRequestId::encode(TT &bl) const {
  using ceph::encode;
  encode(client_id, bl);
  encode(request_id, bl);
}
template void AsyncRequestId::encode<bufferlist&>(bufferlist &bl) const;
template void AsyncRequestId::encode<encode_size&>(encode_size &bl) const;
template void AsyncRequestId::encode<encode_helper&>(encode_helper &bl) const;

void AsyncRequestId::decode(bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(client_id, iter);
  decode(request_id, iter);
}

void AsyncRequestId::dump(Formatter *f) const {
  f->open_object_section("client_id");
  client_id.dump(f);
  f->close_section();
  f->dump_unsigned("request_id", request_id);
}

template <class TT> void AcquiredLockPayload::encode(TT &bl) const {
  using ceph::encode;
  encode(client_id, bl);
}
template void AcquiredLockPayload::encode<bufferlist&>(bufferlist &bl) const;
template void AcquiredLockPayload::encode<encode_size&>(encode_size &bl) const;
template void AcquiredLockPayload::encode<encode_helper&>(encode_helper &bl) const;

void AcquiredLockPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  if (version >= 2) {
    decode(client_id, iter);
  }
}

void AcquiredLockPayload::dump(Formatter *f) const {
  f->open_object_section("client_id");
  client_id.dump(f);
  f->close_section();
}

template <class TT> void ReleasedLockPayload::encode(TT &bl) const {
  using ceph::encode;
  encode(client_id, bl);
}
template void ReleasedLockPayload::encode<bufferlist&>(bufferlist &bl) const;
template void ReleasedLockPayload::encode<encode_size&>(encode_size &bl) const;
template void ReleasedLockPayload::encode<encode_helper&>(encode_helper &bl) const;

void ReleasedLockPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  if (version >= 2) {
    decode(client_id, iter);
  }
}

void ReleasedLockPayload::dump(Formatter *f) const {
  f->open_object_section("client_id");
  client_id.dump(f);
  f->close_section();
}

template <class TT> void RequestLockPayload::encode(TT &bl) const {
  using ceph::encode;
  encode(client_id, bl);
  encode(force, bl);
}
template void RequestLockPayload::encode<bufferlist&>(bufferlist &bl) const;
template void RequestLockPayload::encode<encode_size&>(encode_size &bl) const;
template void RequestLockPayload::encode<encode_helper&>(encode_helper &bl) const;

void RequestLockPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  if (version >= 2) {
    decode(client_id, iter);
  }
  if (version >= 3) {
    decode(force, iter);
  }
}

void RequestLockPayload::dump(Formatter *f) const {
  f->open_object_section("client_id");
  client_id.dump(f);
  f->close_section();
  f->dump_bool("force", force);
}

template <class TT> void HeaderUpdatePayload::encode(TT &bl) const {
}
template void HeaderUpdatePayload::encode<bufferlist&>(bufferlist &bl) const;
template void HeaderUpdatePayload::encode<encode_size&>(encode_size &bl) const;
template void HeaderUpdatePayload::encode<encode_helper&>(encode_helper &bl) const;

void HeaderUpdatePayload::decode(__u8 version, bufferlist::const_iterator &iter) {
}

void HeaderUpdatePayload::dump(Formatter *f) const {
}

template <class TT> void AsyncRequestPayloadBase::encode(TT &bl) const {
  using ceph::encode;
  encode(async_request_id, bl);
}
template void AsyncRequestPayloadBase::encode<bufferlist&>(bufferlist &bl) const;
template void AsyncRequestPayloadBase::encode<encode_size&>(encode_size &bl) const;
template void AsyncRequestPayloadBase::encode<encode_helper&>(encode_helper &bl) const;

void AsyncRequestPayloadBase::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(async_request_id, iter);
}

void AsyncRequestPayloadBase::dump(Formatter *f) const {
  f->open_object_section("async_request_id");
  async_request_id.dump(f);
  f->close_section();
}

template <class TT> void AsyncProgressPayload::encode(TT &bl) const {
  using ceph::encode;
  AsyncRequestPayloadBase::encode(bl);
  encode(offset, bl);
  encode(total, bl);
}
template void AsyncProgressPayload::encode<bufferlist&>(bufferlist &bl) const;
template void AsyncProgressPayload::encode<encode_size&>(encode_size &bl) const;
template void AsyncProgressPayload::encode<encode_helper&>(encode_helper &bl) const;

void AsyncProgressPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  AsyncRequestPayloadBase::decode(version, iter);
  decode(offset, iter);
  decode(total, iter);
}

void AsyncProgressPayload::dump(Formatter *f) const {
  AsyncRequestPayloadBase::dump(f);
  f->dump_unsigned("offset", offset);
  f->dump_unsigned("total", total);
}

template <class TT> void AsyncCompletePayload::encode(TT &bl) const {
  using ceph::encode;
  AsyncRequestPayloadBase::encode(bl);
  encode(result, bl);
}
template void AsyncCompletePayload::encode<bufferlist&>(bufferlist &bl) const;
template void AsyncCompletePayload::encode<encode_size&>(encode_size &bl) const;
template void AsyncCompletePayload::encode<encode_helper&>(encode_helper &bl) const;

void AsyncCompletePayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  AsyncRequestPayloadBase::decode(version, iter);
  decode(result, iter);
}

void AsyncCompletePayload::dump(Formatter *f) const {
  AsyncRequestPayloadBase::dump(f);
  f->dump_int("result", result);
}

template <class TT> void ResizePayload::encode(TT &bl) const {
  using ceph::encode;
  encode(size, bl);
  AsyncRequestPayloadBase::encode(bl);
  encode(allow_shrink, bl);
}
template void ResizePayload::encode<bufferlist&>(bufferlist &bl) const;
template void ResizePayload::encode<encode_size&>(encode_size &bl) const;
template void ResizePayload::encode<encode_helper&>(encode_helper &bl) const;

void ResizePayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(size, iter);
  AsyncRequestPayloadBase::decode(version, iter);

  if (version >= 4) {
    decode(allow_shrink, iter);
  }
}

void ResizePayload::dump(Formatter *f) const {
  f->dump_unsigned("size", size);
  f->dump_bool("allow_shrink", allow_shrink);
  AsyncRequestPayloadBase::dump(f);
}

template <class TT> void SnapPayloadBase::encode(TT &bl) const {
  using ceph::encode;
  encode(snap_name, bl);
  encode(snap_namespace, bl);
}
template void SnapPayloadBase::encode<bufferlist&>(bufferlist &bl) const;
template void SnapPayloadBase::encode<encode_size&>(encode_size &bl) const;
template void SnapPayloadBase::encode<encode_helper&>(encode_helper &bl) const;

void SnapPayloadBase::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(snap_name, iter);
  if (version >= 6) {
    decode(snap_namespace, iter);
  }
}

void SnapPayloadBase::dump(Formatter *f) const {
  f->dump_string("snap_name", snap_name);
  snap_namespace.dump(f);
}

template <class TT> void SnapCreatePayload::encode(TT &bl) const {
  SnapPayloadBase::encode(bl);
}
template void SnapCreatePayload::encode<bufferlist&>(bufferlist &bl) const;
template void SnapCreatePayload::encode<encode_size&>(encode_size &bl) const;
template void SnapCreatePayload::encode<encode_helper&>(encode_helper &bl) const;

void SnapCreatePayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  SnapPayloadBase::decode(version, iter);
  if (version == 5) {
    decode(snap_namespace, iter);
  }
}

void SnapCreatePayload::dump(Formatter *f) const {
  SnapPayloadBase::dump(f);
}

template <class TT> void SnapRenamePayload::encode(TT &bl) const {
  using ceph::encode;
  encode(snap_id, bl);
  SnapPayloadBase::encode(bl);
}
template void SnapRenamePayload::encode<bufferlist&>(bufferlist &bl) const;
template void SnapRenamePayload::encode<encode_size&>(encode_size &bl) const;
template void SnapRenamePayload::encode<encode_helper&>(encode_helper &bl) const;

void SnapRenamePayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(snap_id, iter);
  SnapPayloadBase::decode(version, iter);
}

void SnapRenamePayload::dump(Formatter *f) const {
  f->dump_unsigned("src_snap_id", snap_id);
  SnapPayloadBase::dump(f);
}

template <class TT> void RenamePayload::encode(TT &bl) const {
  using ceph::encode;
  encode(image_name, bl);
}
template void RenamePayload::encode<bufferlist&>(bufferlist &bl) const;
template void RenamePayload::encode<encode_size&>(encode_size &bl) const;
template void RenamePayload::encode<encode_helper&>(encode_helper &bl) const;

void RenamePayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(image_name, iter);
}

void RenamePayload::dump(Formatter *f) const {
  f->dump_string("image_name", image_name);
}

template <class TT> void UpdateFeaturesPayload::encode(TT &bl) const {
  using ceph::encode;
  encode(features, bl);
  encode(enabled, bl);
}
template void UpdateFeaturesPayload::encode<bufferlist&>(bufferlist &bl) const;
template void UpdateFeaturesPayload::encode<encode_size&>(encode_size &bl) const;
template void UpdateFeaturesPayload::encode<encode_helper&>(encode_helper &bl) const;

void UpdateFeaturesPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  decode(features, iter);
  decode(enabled, iter);
}

void UpdateFeaturesPayload::dump(Formatter *f) const {
  f->dump_unsigned("features", features);
  f->dump_bool("enabled", enabled);
}

template <class TT> void UnknownPayload::encode(TT &bl) const {
  ceph_abort();
}
template void UnknownPayload::encode<bufferlist&>(bufferlist &bl) const;
template void UnknownPayload::encode<encode_size&>(encode_size &bl) const;
template void UnknownPayload::encode<encode_helper&>(encode_helper &bl) const;

void UnknownPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
}

void UnknownPayload::dump(Formatter *f) const {
}

bool NotifyMessage::check_for_refresh() const {
  return boost::apply_visitor(CheckForRefreshVisitor(), payload);
}

template <class TT> void NotifyMessage::encode(TT& bl) const {
  ENCODE_START(6, 1, bl);
  boost::apply_visitor(watcher::util::EncodePayloadVisitor(bl), payload);
  ENCODE_FINISH(bl);
}
template void NotifyMessage::encode<bufferlist&>(bufferlist& bl) const;
template void NotifyMessage::encode<encode_size&>(encode_size& bl) const;
template void NotifyMessage::encode<encode_helper&>(encode_helper& bl) const;

void NotifyMessage::decode(bufferlist::const_iterator& iter) {
  DECODE_START(1, iter);

  uint32_t notify_op;
  decode(notify_op, iter);

  // select the correct payload variant based upon the encoded op
  switch (notify_op) {
  case NOTIFY_OP_ACQUIRED_LOCK:
    payload = AcquiredLockPayload();
    break;
  case NOTIFY_OP_RELEASED_LOCK:
    payload = ReleasedLockPayload();
    break;
  case NOTIFY_OP_REQUEST_LOCK:
    payload = RequestLockPayload();
    break;
  case NOTIFY_OP_HEADER_UPDATE:
    payload = HeaderUpdatePayload();
    break;
  case NOTIFY_OP_ASYNC_PROGRESS:
    payload = AsyncProgressPayload();
    break;
  case NOTIFY_OP_ASYNC_COMPLETE:
    payload = AsyncCompletePayload();
    break;
  case NOTIFY_OP_FLATTEN:
    payload = FlattenPayload();
    break;
  case NOTIFY_OP_RESIZE:
    payload = ResizePayload();
    break;
  case NOTIFY_OP_SNAP_CREATE:
    payload = SnapCreatePayload();
    break;
  case NOTIFY_OP_SNAP_REMOVE:
    payload = SnapRemovePayload();
    break;
  case NOTIFY_OP_SNAP_RENAME:
    payload = SnapRenamePayload();
    break;
  case NOTIFY_OP_SNAP_PROTECT:
    payload = SnapProtectPayload();
    break;
  case NOTIFY_OP_SNAP_UNPROTECT:
    payload = SnapUnprotectPayload();
    break;
  case NOTIFY_OP_REBUILD_OBJECT_MAP:
    payload = RebuildObjectMapPayload();
    break;
  case NOTIFY_OP_RENAME:
    payload = RenamePayload();
    break;
  case NOTIFY_OP_UPDATE_FEATURES:
    payload = UpdateFeaturesPayload();
    break;
  case NOTIFY_OP_MIGRATE:
    payload = MigratePayload();
    break;
  default:
    payload = UnknownPayload();
    break;
  }

  apply_visitor(watcher::util::DecodePayloadVisitor(struct_v, iter), payload);
  DECODE_FINISH(iter);
}

void NotifyMessage::dump(Formatter *f) const {
  apply_visitor(DumpPayloadVisitor(f), payload);
}

NotifyOp NotifyMessage::get_notify_op() const {
  return apply_visitor(GetNotifyOpVisitor(), payload);
}

void NotifyMessage::generate_test_instances(std::list<NotifyMessage *> &o) {
  o.push_back(new NotifyMessage(AcquiredLockPayload(ClientId(1, 2))));
  o.push_back(new NotifyMessage(ReleasedLockPayload(ClientId(1, 2))));
  o.push_back(new NotifyMessage(RequestLockPayload(ClientId(1, 2), true)));
  o.push_back(new NotifyMessage(HeaderUpdatePayload()));
  o.push_back(new NotifyMessage(AsyncProgressPayload(AsyncRequestId(ClientId(0, 1), 2), 3, 4)));
  o.push_back(new NotifyMessage(AsyncCompletePayload(AsyncRequestId(ClientId(0, 1), 2), 3)));
  o.push_back(new NotifyMessage(FlattenPayload(AsyncRequestId(ClientId(0, 1), 2))));
  o.push_back(new NotifyMessage(ResizePayload(123, true, AsyncRequestId(ClientId(0, 1), 2))));
  o.push_back(new NotifyMessage(SnapCreatePayload(cls::rbd::UserSnapshotNamespace(),
						  "foo")));
  o.push_back(new NotifyMessage(SnapRemovePayload(cls::rbd::UserSnapshotNamespace(), "foo")));
  o.push_back(new NotifyMessage(SnapProtectPayload(cls::rbd::UserSnapshotNamespace(), "foo")));
  o.push_back(new NotifyMessage(SnapUnprotectPayload(cls::rbd::UserSnapshotNamespace(), "foo")));
  o.push_back(new NotifyMessage(RebuildObjectMapPayload(AsyncRequestId(ClientId(0, 1), 2))));
  o.push_back(new NotifyMessage(RenamePayload("foo")));
  o.push_back(new NotifyMessage(UpdateFeaturesPayload(1, true)));
  o.push_back(new NotifyMessage(MigratePayload(AsyncRequestId(ClientId(0, 1), 2))));
}

template <class TT> void ResponseMessage::encode(TT& bl) const {
  ENCODE_START(1, 1, bl);
  encode(result, bl);
  ENCODE_FINISH(bl);
}
template void ResponseMessage::encode<bufferlist&>(bufferlist& bl) const;
template void ResponseMessage::encode<encode_size&>(encode_size& bl) const;
template void ResponseMessage::encode<encode_helper&>(encode_helper& bl) const;

void ResponseMessage::decode(bufferlist::const_iterator& iter) {
  DECODE_START(1, iter);
  decode(result, iter);
  DECODE_FINISH(iter);
}

void ResponseMessage::dump(Formatter *f) const {
  f->dump_int("result", result);
}

void ResponseMessage::generate_test_instances(std::list<ResponseMessage *> &o) {
  o.push_back(new ResponseMessage(1));
}

std::ostream &operator<<(std::ostream &out,
                         const librbd::watch_notify::NotifyOp &op) {
  using namespace librbd::watch_notify;

  switch (op) {
  case NOTIFY_OP_ACQUIRED_LOCK:
    out << "AcquiredLock";
    break;
  case NOTIFY_OP_RELEASED_LOCK:
    out << "ReleasedLock";
    break;
  case NOTIFY_OP_REQUEST_LOCK:
    out << "RequestLock";
    break;
  case NOTIFY_OP_HEADER_UPDATE:
    out << "HeaderUpdate";
    break;
  case NOTIFY_OP_ASYNC_PROGRESS:
    out << "AsyncProgress";
    break;
  case NOTIFY_OP_ASYNC_COMPLETE:
    out << "AsyncComplete";
    break;
  case NOTIFY_OP_FLATTEN:
    out << "Flatten";
    break;
  case NOTIFY_OP_RESIZE:
    out << "Resize";
    break;
  case NOTIFY_OP_SNAP_CREATE:
    out << "SnapCreate";
    break;
  case NOTIFY_OP_SNAP_REMOVE:
    out << "SnapRemove";
    break;
  case NOTIFY_OP_SNAP_RENAME:
    out << "SnapRename";
    break;
  case NOTIFY_OP_SNAP_PROTECT:
    out << "SnapProtect";
    break;
  case NOTIFY_OP_SNAP_UNPROTECT:
    out << "SnapUnprotect";
    break;
  case NOTIFY_OP_REBUILD_OBJECT_MAP:
    out << "RebuildObjectMap";
    break;
  case NOTIFY_OP_RENAME:
    out << "Rename";
    break;
  case NOTIFY_OP_UPDATE_FEATURES:
    out << "UpdateFeatures";
    break;
  case NOTIFY_OP_MIGRATE:
    out << "Migrate";
    break;
  default:
    out << "Unknown (" << static_cast<uint32_t>(op) << ")";
    break;
  }
  return out;
}

std::ostream &operator<<(std::ostream &out,
                         const librbd::watch_notify::AsyncRequestId &request) {
  out << "[" << request.client_id.gid << "," << request.client_id.handle << ","
      << request.request_id << "]";
  return out;
}
} // namespace watch_notify
} // namespace librbd
