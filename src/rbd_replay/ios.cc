// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2014 Adam Crume <adamcrume@gmail.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

// This code assumes that IO IDs and timestamps are related monotonically.
// In other words, (a.id < b.id) == (a.timestamp < b.timestamp) for all IOs a and b.

#include "ios.hpp"
#include "rbd_replay/ActionTypes.h"

using namespace std;
using namespace rbd_replay;

namespace {

bool compare_dependencies_by_start_time(const action::Dependency &lhs,
                                        const action::Dependency &rhs) {
  return lhs.time_delta < rhs.time_delta;
}

action::Dependencies convert_dependencies(uint64_t start_time,
                                          const io_set_t &deps) {
  action::Dependencies action_deps;
  action_deps.reserve(deps.size());
  for (io_set_t::const_iterator it = deps.begin(); it != deps.end(); ++it) {
    boost::shared_ptr<IO> io = *it;
    uint64_t time_delta = 0;
    if (start_time >= io->start_time()) {
      time_delta = start_time - io->start_time();
    }
    action_deps.push_back(action::Dependency(io->ionum(), time_delta));
  }
  std::sort(action_deps.begin(), action_deps.end(),
            compare_dependencies_by_start_time);
  return action_deps;
}

} // anonymous namespace

void IO::write_debug_base(ostream& out, const string &type) const {
  out << m_ionum << ": " << m_start_time / 1000000.0 << ": " << type << ", thread = " << m_thread_id << ", deps = {";
  bool first = true;
  for (io_set_t::iterator itr = m_dependencies.begin(), end = m_dependencies.end(); itr != end; ++itr) {
    if (first) {
      first = false;
    } else {
      out << ", ";
    }
    out << (*itr)->m_ionum << ": " << m_start_time - (*itr)->m_start_time;
  }
  out << "}";
}


ostream& operator<<(ostream &out, const IO::ptr &io) {
  io->write_debug(out);
  return out;
}

template <class TT> void StartThreadIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::StartThreadAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()))));
  encode(action, bl);
}
template void StartThreadIO::encode<bufferlist>(bufferlist &bl) const;
template void StartThreadIO::encode<encode_size>(encode_size &bl) const;
template void StartThreadIO::encode<encode_helper>(encode_helper &bl) const;

void StartThreadIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "start thread");
}

template <class TT> void StopThreadIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::StopThreadAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()))));
  encode(action, bl);
}
template void StopThreadIO::encode<bufferlist>(bufferlist &bl) const;
template void StopThreadIO::encode<encode_size>(encode_size &bl) const;
template void StopThreadIO::encode<encode_helper>(encode_helper &bl) const;

void StopThreadIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "stop thread");
}

template <class TT> void ReadIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::ReadAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx, m_offset, m_length)));
  encode(action, bl);
}
template void ReadIO::encode<bufferlist>(bufferlist &bl) const;
template void ReadIO::encode<encode_size>(encode_size &bl) const;
template void ReadIO::encode<encode_helper>(encode_helper &bl) const;

void ReadIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "read");
  out << ", imagectx=" << m_imagectx << ", offset=" << m_offset << ", length=" << m_length << "]";
}

template <class TT> void WriteIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::WriteAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx, m_offset, m_length)));
  encode(action, bl);
}
template void WriteIO::encode<bufferlist>(bufferlist &bl) const;
template void WriteIO::encode<encode_size>(encode_size &bl) const;
template void WriteIO::encode<encode_helper>(encode_helper &bl) const;

void WriteIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "write");
  out << ", imagectx=" << m_imagectx << ", offset=" << m_offset << ", length=" << m_length << "]";
}

template <class TT> void DiscardIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::DiscardAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx, m_offset, m_length)));
  encode(action, bl);
}
template void DiscardIO::encode<bufferlist>(bufferlist &bl) const;
template void DiscardIO::encode<encode_size>(encode_size &bl) const;
template void DiscardIO::encode<encode_helper>(encode_helper &bl) const;

void DiscardIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "discard");
  out << ", imagectx=" << m_imagectx << ", offset=" << m_offset << ", length=" << m_length << "]";
}

template <class TT> void AioReadIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::AioReadAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx, m_offset, m_length)));
  encode(action, bl);
}
template void AioReadIO::encode<bufferlist>(bufferlist &bl) const;
template void AioReadIO::encode<encode_size>(encode_size &bl) const;
template void AioReadIO::encode<encode_helper>(encode_helper &bl) const;

void AioReadIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "aio read");
  out << ", imagectx=" << m_imagectx << ", offset=" << m_offset << ", length=" << m_length << "]";
}

template <class TT> void AioWriteIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::AioWriteAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx, m_offset, m_length)));
  encode(action, bl);
}
template void AioWriteIO::encode<bufferlist>(bufferlist &bl) const;
template void AioWriteIO::encode<encode_size>(encode_size &bl) const;
template void AioWriteIO::encode<encode_helper>(encode_helper &bl) const;

void AioWriteIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "aio write");
  out << ", imagectx=" << m_imagectx << ", offset=" << m_offset << ", length=" << m_length << "]";
}

template <class TT> void AioDiscardIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::AioDiscardAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx, m_offset, m_length)));
  encode(action, bl);
}
template void AioDiscardIO::encode<bufferlist>(bufferlist &bl) const;
template void AioDiscardIO::encode<encode_size>(encode_size &bl) const;
template void AioDiscardIO::encode<encode_helper>(encode_helper &bl) const;

void AioDiscardIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "aio discard");
  out << ", imagectx=" << m_imagectx << ", offset=" << m_offset << ", length=" << m_length << "]";
}

template <class TT> void OpenImageIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::OpenImageAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx, m_name, m_snap_name, m_readonly)));
  encode(action, bl);
}
template void OpenImageIO::encode<bufferlist>(bufferlist &bl) const;
template void OpenImageIO::encode<encode_size>(encode_size &bl) const;
template void OpenImageIO::encode<encode_helper>(encode_helper &bl) const;

void OpenImageIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "open image");
  out << ", imagectx=" << m_imagectx << ", name='" << m_name << "', snap_name='" << m_snap_name << "', readonly=" << m_readonly;
}

template <class TT> void CloseImageIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::CloseImageAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx)));
  encode(action, bl);
}
template void CloseImageIO::encode<bufferlist>(bufferlist &bl) const;
template void CloseImageIO::encode<encode_size>(encode_size &bl) const;
template void CloseImageIO::encode<encode_helper>(encode_helper &bl) const;

void CloseImageIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "close image");
  out << ", imagectx=" << m_imagectx;
}

template <class TT> void AioOpenImageIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::AioOpenImageAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx, m_name, m_snap_name, m_readonly)));
  encode(action, bl);
}
template void AioOpenImageIO::encode<bufferlist>(bufferlist &bl) const;
template void AioOpenImageIO::encode<encode_size>(encode_size &bl) const;
template void AioOpenImageIO::encode<encode_helper>(encode_helper &bl) const;

void AioOpenImageIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "aio open image");
  out << ", imagectx=" << m_imagectx << ", name='" << m_name << "', snap_name='" << m_snap_name << "', readonly=" << m_readonly;
}

template <class TT> void AioCloseImageIO::encode(TT &bl) const {
  using ceph::encode;
  action::Action action((action::AioCloseImageAction(
    ionum(), thread_id(), convert_dependencies(start_time(), dependencies()),
    m_imagectx)));
  encode(action, bl);
}
template void AioCloseImageIO::encode<bufferlist>(bufferlist &bl) const;
template void AioCloseImageIO::encode<encode_size>(encode_size &bl) const;
template void AioCloseImageIO::encode<encode_helper>(encode_helper &bl) const;

void AioCloseImageIO::write_debug(std::ostream& out) const {
  write_debug_base(out, "aio close image");
  out << ", imagectx=" << m_imagectx;
}
