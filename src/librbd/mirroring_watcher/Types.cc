// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "common/Formatter.h"
#include "include/ceph_assert.h"
#include "include/stringify.h"
#include "librbd/mirroring_watcher/Types.h"
#include "librbd/watcher/Utils.h"

namespace librbd {
namespace mirroring_watcher {

namespace {

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

template <class TT> void ModeUpdatedPayload::encode(TT &bl) const {
  using ceph::encode;
  encode(static_cast<uint32_t>(mirror_mode), bl);
}
template void ModeUpdatedPayload::encode<bufferlist>(bufferlist &bl) const;
template void ModeUpdatedPayload::encode<encode_size>(encode_size &bl) const;
template void ModeUpdatedPayload::encode<encode_helper>(encode_helper &bl) const;

void ModeUpdatedPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  uint32_t mirror_mode_decode;
  decode(mirror_mode_decode, iter);
  mirror_mode = static_cast<cls::rbd::MirrorMode>(mirror_mode_decode);
}

void ModeUpdatedPayload::dump(Formatter *f) const {
  f->dump_stream("mirror_mode") << mirror_mode;
}

template <class TT> void ImageUpdatedPayload::encode(TT &bl) const {
  using ceph::encode;
  encode(static_cast<uint32_t>(mirror_image_state), bl);
  encode(image_id, bl);
  encode(global_image_id, bl);
}
template void ImageUpdatedPayload::encode<bufferlist>(bufferlist &bl) const;
template void ImageUpdatedPayload::encode<encode_size>(encode_size &bl) const;
template void ImageUpdatedPayload::encode<encode_helper>(encode_helper &bl) const;

void ImageUpdatedPayload::decode(__u8 version, bufferlist::const_iterator &iter) {
  using ceph::decode;
  uint32_t mirror_image_state_decode;
  decode(mirror_image_state_decode, iter);
  mirror_image_state = static_cast<cls::rbd::MirrorImageState>(
    mirror_image_state_decode);
  decode(image_id, iter);
  decode(global_image_id, iter);
}

void ImageUpdatedPayload::dump(Formatter *f) const {
  f->dump_stream("mirror_image_state") << mirror_image_state;
  f->dump_string("image_id", image_id);
  f->dump_string("global_image_id", global_image_id);
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
  ENCODE_START(1, 1, bl);
  boost::apply_visitor(watcher::util::EncodePayloadVisitor(bl), payload);
  ENCODE_FINISH(bl);
}
template void NotifyMessage::encode<bufferlist>(bufferlist& bl) const;
template void NotifyMessage::encode<encode_size>(encode_size& bl) const;
template void NotifyMessage::encode<encode_helper>(encode_helper& bl) const;

void NotifyMessage::decode(bufferlist::const_iterator& iter) {
  DECODE_START(1, iter);

  uint32_t notify_op;
  decode(notify_op, iter);

  // select the correct payload variant based upon the encoded op
  switch (notify_op) {
  case NOTIFY_OP_MODE_UPDATED:
    payload = ModeUpdatedPayload();
    break;
  case NOTIFY_OP_IMAGE_UPDATED:
    payload = ImageUpdatedPayload();
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

void NotifyMessage::generate_test_instances(std::list<NotifyMessage *> &o) {
  o.push_back(new NotifyMessage(ModeUpdatedPayload(cls::rbd::MIRROR_MODE_DISABLED)));
  o.push_back(new NotifyMessage(ImageUpdatedPayload(cls::rbd::MIRROR_IMAGE_STATE_DISABLING,
                                                    "image id", "global image id")));
}

std::ostream &operator<<(std::ostream &out, const NotifyOp &op) {
  switch (op) {
  case NOTIFY_OP_MODE_UPDATED:
    out << "ModeUpdated";
    break;
  case NOTIFY_OP_IMAGE_UPDATED:
    out << "ImageUpdated";
    break;
  default:
    out << "Unknown (" << static_cast<uint32_t>(op) << ")";
    break;
  }
  return out;
}

} // namespace mirroring_watcher
} // namespace librbd
