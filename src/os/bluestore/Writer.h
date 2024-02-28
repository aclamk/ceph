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
 */

#ifndef BLUESTORE_WRITER
#define BLUESTORE_WRITER

#include "BlueStore.h"
#include "Allocator.h"

class BlueStore::Writer {
public:
  using exmp_it = extent_map_t::iterator;
  using P = BlueStore::printer;

  struct blob_data {
    uint32_t real_length;
    uint32_t compressed_length; // defines compressed
    bufferlist disk_data; //always at least block aligned
    bufferlist object_data;
    bool is_compressed() {return compressed_length != 0;}
  };

  struct write_divertor {
    virtual ~write_divertor() = default;
    virtual void write(
      uint64_t disk_offset, const bufferlist& data, bool deferred) = 0;
  };
  struct read_divertor {
    virtual ~read_divertor() = default;
    virtual bufferlist read(uint32_t object_offset, uint32_t object_length) = 0;
  };
  BlueStore* bstore;
  TransContext* txc;
  WriteContext* wctx;
  OnodeRef o;
  write_divertor* test_write_divertor;
  read_divertor* test_read_divertor;
  PExtentVector released;   //filled by punch_hole
  PExtentVector allocated;  //filled by alloc()
  std::vector<BlobRef> pruned_blobs;
  std::set<SharedBlobRef> shared_changed;
  volatile_statfs statfs_delta;
  bool do_deferred = false;
  struct {
    PExtentVector::iterator it;  //iterator
    uint32_t                pos; //in-iterator position
  } disk_allocs; //disk locations to use when placing data
  uint16_t pp_mode = 0; //pretty print mode
  uint16_t debug_level_to_pp_mode(CephContext* cct);

  inline exmp_it _find_mutable_blob_left(
    BlueStore::extent_map_t::iterator it,
    uint32_t search_begin, // only interested in blobs that are
    uint32_t search_end,   // within range [begin - end)
    uint32_t mapmust_begin,// for 'unused' case: the area
    uint32_t mapmust_end); // [begin - end) must be mapped 

  inline exmp_it _find_mutable_blob_right(
    exmp_it it,
    uint32_t search_begin,  // only interested in blobs that are
    uint32_t search_end,    // within range [begin - end)
    uint32_t mapmust_begin, // for 'unused' case: the area
    uint32_t mapmust_end);  // [begin - end) must be mapped 

  inline void _schedule_io_masked(
    uint64_t disk_offset,
    bufferlist data,
    bluestore_blob_t::unused_t mask,
    uint32_t chunk_size);

  inline void _schedule_io(
    const PExtentVector& disk_allocs,
    uint32_t initial_offset,
    bufferlist data);

  //Take `length` space from `this.disk_allocs` and put it to `dst`.
  void _get_disk_space(
    uint32_t length,
    PExtentVector& dst);

  inline bufferlist _read_self(
    uint32_t offset,
    uint32_t length);

  inline void _maybe_expand_blob(
    Blob* blob,
    uint32_t new_blob_size);

  inline void _blob_put_data(
    Blob* blob,
    uint32_t in_blob_offset,
    bufferlist disk_data);

  inline void _blob_put_data_subau(
    Blob* blob,
    uint32_t in_blob_offset,
    bufferlist disk_data);

  inline void _blob_put_data_allocate(
    Blob* blob,
    uint32_t in_blob_offset,
    bufferlist disk_data);

  inline void _blob_put_data_subau_allocate(
    Blob* blob,
    uint32_t in_blob_offset,
    bufferlist disk_data);

  BlobRef _blob_create_with_data(
    uint32_t in_blob_offset,
    bufferlist& disk_data);

  BlobRef _blob_create_full(
    bufferlist& disk_data);

  void _try_reuse_allocated_l(
    exmp_it after_punch_it,   // hint, we could have found it ourselves
    uint32_t& logical_offset, // will fix value if something consumed
    uint32_t ref_end_offset,  // useful when data is padded
    blob_data& bd);           // modified when consumed

  void _try_reuse_allocated_r(
    exmp_it after_punch_it,   // hint, we could have found it ourselves
    uint32_t& end_offset,     // will fix value if something consumed
    uint32_t ref_end_offset,  // useful when data is padded
    blob_data& bd);           // modified when consumed

  void _try_put_data_on_allocated(
  uint32_t& logical_offset, 
  uint32_t& end_offset,
  uint32_t& ref_end_offset,
  std::vector<blob_data>& bd,
  exmp_it after_punch_it);

  void _do_put_new_blobs(
    uint32_t logical_offset, 
    uint32_t ref_end_offset,
    std::vector<blob_data>::iterator& bd_it,
    std::vector<blob_data>::iterator bd_end);

  void _do_put_blobs(
    uint32_t logical_offset, 
    uint32_t data_end_offset,
    uint32_t ref_end_offset,
    std::vector<blob_data>& bd,
    exmp_it after_punch_it);

  std::pair<bool, uint32_t> _write_expand_l(
    uint32_t logical_offset);

  std::pair<bool, uint32_t> _write_expand_r(
    uint32_t end_offset);

  void _do_write(
    uint32_t logical_offset,
    bufferlist& data
  );

  void _debug_iterate_buffers(
    std::function<void(uint32_t offset, const bufferlist& data)> data_callback
  );
};
#endif // BLUESTORE_WRITER