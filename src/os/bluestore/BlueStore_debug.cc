// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2023 IBM
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */
#include <cstdint>
#include <ostream>
#include "BlueStore.h"

static const std::string transition_table[26] = {
"bcdfghjklmnprstuvxyz", //a
"aeiloruy",//b
"aeiloruvy",//c
"aeilmnoruvy",//d
"bcdfghjklmnprstvxz",//e

"ailou",//f
"aeilnoru",//g
"aeiloru",//h
"dfghklmnpqrstvwx",//i
"aeiou",//j

"aeiloru",//k
"aeimnou",//l
"aeinotuy",//m
"aeiou",//n
"bcdfghjklmnpqrstvwxz",//o
"aehiloruy",//p

"aeiloru",//q
"adefiklmnotuvy",//r
"aehiklmnopqrtuvwy",//s
"acefhiklmnorsuvwy",//t
"bcdfghklmnpqrsvwxyz",//u

"acdeiklmorsu",//v
"aehilnorstu",//w
"aeilnorstuy",//x
"aehinorsuxz",//y
"aeiouy" //z
};

std::string int_to_fancy_name(uint64_t x)
{
  std::string result;
  uint8_t c = x % 26;
  x = x / 26;
  result.push_back(c+'a');
  while (x > 0) {
    uint8_t range = transition_table[c].length();
    uint8_t p = x % range;
    c = transition_table[c][p] - 'a';
    x = x / range;
    result.push_back(c+'a');
  }
  return result;
}

// Use special printing for multiplies of 1024; print K suffix
struct maybe_K {
  uint32_t x;
  maybe_K(uint32_t x) : x(x) {}
};
std::ostream &operator<<(std::ostream &out, const maybe_K &k) {
  if (((k.x & 0x3ff) == 0) && (k.x != 0)) {
    if (k.x != 0x400)
      out << (k.x / 0x400);
    out << "K";
  } else {
    out << std::hex << k.x << std::dec;
  }
  return out;
}
// cheap, not very reliable but portable detector where heap starts
static std::unique_ptr<char> heap_begin(new char);
std::ostream& operator<<(std::ostream& out, const BlueStore::Buffer& b);
std::ostream& operator<<(std::ostream& out, const BlueStore::Blob::printer &p)
{
  using P = BlueStore::printer;
  out << "Blob(";
  if (p.mode & P::PTR) {
    out << &p.blob << " ";
  }
  if (p.mode & P::NICK) {
    uint64_t v = uint64_t(&p.blob);
    //Assume allocated Blobs will be 16 bytes aligned.
    v = (v - (uintptr_t)heap_begin.get()) / 16;
    out << int_to_fancy_name(v) << " ";
  }
  const bluestore_blob_t& bblob = p.blob.get_blob();
  if (p.mode & P::DISK) {
    //use default printer for std::vector * bluestore_pextent_t
    out << "disk=" << bblob.get_extents() << " ";
  }
  if (p.mode & P::SDISK) {
    const PExtentVector& ev = bblob.get_extents();
    uint64_t bits = 0;
    for (auto i : ev) {
      if (i.is_valid()) bits |= i.offset;
      bits |= i.length;
    }
    uint32_t zeros = 0; //zeros to apply to all values
    while ((bits & 0xf) == 0) {
      bits = bits >> 4;
      ++zeros;
    }
    out << "disk=0x[" << std::hex;
    for (size_t i = 0; i < ev.size(); ++i) {
      if (i != 0) {
        out << ",";
      }
      if (ev[i].is_valid()) {
        out << (ev[i].offset >> zeros * 4) << "~";
      } else {
        out << "!";
      }
      out << (ev[i].length >> zeros * 4);
    }
    out << "]" << std::dec;
    while (zeros > 0) {
      out << "0";
      --zeros;
    }
    out << " ";
  }
  //always print lengths, if not printing use tracker
  if (!(p.mode & (P::USE | P::SUSE)) || bblob.is_compressed()) {
    // Need to print blob logical length, no tracker printing
    // + there is no real tracker for compressed blobs
    if (bblob.is_compressed()) {
      out << "len=" << std::hex << bblob.get_logical_length() << "->"
          << bblob.get_compressed_payload_length() << " " << std::dec;
    } else {
      out << "len=" << std::hex << bblob.get_logical_length() << std::dec << " ";
    }
  }
  if ((p.mode & P::USE) && !bblob.is_compressed()) {
    out << p.blob.get_blob_use_tracker() << " ";
  }
  if (p.mode & P::SUSE) {
    auto& tracker = p.blob.get_blob_use_tracker();
    if (bblob.is_compressed()) {
      out << "[" << std::hex << tracker.get_referenced_bytes() << std::dec << "] ";
    } else {
      const uint32_t* au_array = tracker.get_au_array();
      uint16_t zeros = 0;
      uint16_t full = 0;
      uint16_t num_au = tracker.get_num_au();
      uint32_t au_size = tracker.au_size;
      uint32_t def = std::numeric_limits<uint32_t>::max();
      out << "track=" << tracker.get_num_au() << "*" << maybe_K(tracker.au_size);
      for (size_t i = 0; i < num_au; i++) {
        if (au_array[i] == 0) ++zeros;
        if (au_array[i] == au_size) ++full;
      }
      if (zeros >= num_au - 3 && num_au > 6) def = 0;
      if (full >= num_au - 3 && num_au > 6) def = au_size;
      if (def != std::numeric_limits<uint32_t>::max()) {
        out << "{" << maybe_K(def) << "}[";
        for (size_t i = 0; i < num_au; i++) {
          if (au_array[i] != def) {
            out << i << "=" << maybe_K(au_array[i]);
            ++i;
            for (; i < num_au; i++) {
              if (au_array[i] != def) {
                out << "," << i << "=" << maybe_K(au_array[i]);
              }
            }
          }
        }
        out << "] ";
      } else {
        out << "[";
        for (size_t i = 0; i < num_au; i++) {
          if (i != 0) out << ",";
          out << maybe_K(au_array[i]);
        }
        out << "] ";
      }
    }
  }
  if (bblob.has_csum()) {
    if (p.mode & (P::SCHK | P::CHK)) {
      out << Checksummer::get_csum_type_string(bblob.csum_type) << "/"
          << (int)bblob.csum_chunk_order << "/" << bblob.csum_data.length();
    }
    if (p.mode & P::CHK) {
      std::vector<uint64_t> v;
      unsigned n = bblob.get_csum_count();
      for (unsigned i = 0; i < n; ++i)
        v.push_back(bblob.get_csum_item(i));
      out << std::hex << v << std::dec;
    }
    if (p.mode & (P::SCHK | P::CHK)) {
      out << " ";
    }
  }
  if (p.blob.is_spanning()) {
    out << " spanning.id=" << p.blob.id;
  }
  if (p.blob.shared_blob) {
    if (p.blob.shared_blob->get_sbid() != 0) {
      out << " " << *p.blob.shared_blob;
    }
  } else {
    out << " (shared_blob=NULL)";
  }
  out << ")";
  // here printing Buffers
  if (p.mode & (P::BUF | P::SBUF)) {
    std::lock_guard l(p.blob.shared_blob->get_cache()->lock);
    if (p.mode & P::SBUF) {
      // summary buf mode, only print what is mapped what options are, one liner
      out << "bufs(";
      for (auto& i : p.blob.get_bc().buffer_map) {
        out << "0x" << std::hex << i.first << "~" << i.second.length << std::dec
          << "," << BlueStore::Buffer::get_state_name_short(i.second.state);
        if (i.second.flags) {
          out << "," << BlueStore::Buffer::get_flag_name(i.second.flags);
        }
        out << " ";
      }
      out << ")";
    } else {
      for (auto& i : p.blob.get_bc().buffer_map) {
        out << std::endl << "  0x" << std::hex << i.first
          << "~" << i.second.length << std::dec
          << " " << i.second;
      }
    }
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const BlueStore::Extent::printer &p)
{
  out << std::hex << "0x" << p.ext.logical_offset << "~" << p.ext.length
    << ": 0x" << p.ext.blob_offset << "~" << p.ext.length << std::dec
	<< " " << p.ext.blob->print(p.mode);
  return out;
}

std::ostream& operator<<(std::ostream& out, const BlueStore::Onode::printer &p)
{
  using P = BlueStore::printer;
  const BlueStore::Onode& o = p.onode;
  uint16_t mode = p.mode;
  out << &o << " " << o.oid
      << " nid " << o.onode.nid
      << " size 0x" << std::hex << o.onode.size
      << " (" << std::dec << o.onode.size << ")"
      << " expected_object_size " << o.onode.expected_object_size
      << " expected_write_size " << o.onode.expected_write_size
      << " in " << o.onode.extent_map_shards.size() << " shards"
      << ", " << o.extent_map.spanning_blob_map.size()
      << " spanning blobs" << std::endl;
  const BlueStore::ExtentMap& map = o.extent_map;
  std::set<BlueStore::Blob*> visited;
  for (const auto& e : map.extent_map) {
    BlueStore::Blob* b = e.blob.get();
    if (!visited.contains(b)) {
      out << b->print(mode) << std::endl;
      visited.insert(b);
    }
  }
  // to make printing extents in-sync with blobs
  bool mode_extent = mode & (P::PTR | P::NICK);
  for (const auto& e : map.extent_map) {
    out << e.print(mode_extent) << std::endl;
  }
  if (mode & P::ATTRS) {
    for (const auto& p : o.onode.attrs) {
      out << "  attr " << p.first
        << " len " << p.second.length() << std::endl;
    }
  }
  return out;
}
