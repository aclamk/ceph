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

#ifndef COMPRESSION_H_INCLUDED
#define COMPRESSION_H_INCLUDED

#include "BlueStore.h"
#include "Writer.h"

class BlueStore::Estimator {
public:
  Estimator(BlueStore* bluestore)
  :bluestore(bluestore) {}

  // Prepare for new write
  void reset();
  // return estimated size if data gets compressed
  uint32_t estimate(uint32_t new_data);
  // Inform estimator that an extent is a candidate for recompression.
  // Estimator has to calculate (guess) the cost (size) of the referenced data.
  // 'gain' is the size that will be released should extent be recompressed.
  void batch(const BlueStore::Extent* candidate, uint32_t gain);
  // Lets estimator decide if extents previously passed by batch()
  // are worth recompressing.
  // If so (returns true), extents will be added by mark_recompress().
  bool is_worth();
  // Lets estimator decide if an uncompressed neighbor should be
  // recompressed. The extent passed is always uncompressed and
  // always a direct neighbor to already accepted recompression batch.
  // If so (returns true), extents will be added by mark_recompress().
  bool is_worth(const BlueStore::Extent* uncompressed_neighbor);

  void mark_recompress(const BlueStore::Extent* e);
  void mark_main(uint32_t location, uint32_t length);
  struct region_t {
    uint32_t offset; // offset of region
    uint32_t length; // size of region
  };
  void get_regions(std::vector<region_t>& regions);

  void split_and_compress(
    CompressorRef compr,
    uint32_t max_blob_size,
    ceph::buffer::list& data_bl,
    Writer::blob_vec& bd);

private:
  BlueStore* bluestore;
  double expected_compression_factor = 0.5;
  uint32_t min_alloc_size;
  // gain = size on disk (after release or taken if no compression)
  // cost = size estimated on disk after compression
  uint32_t cost = 0;
  uint32_t gain = 0;

  std::map<uint32_t, uint32_t> extra_recompress;
};

class BlueStore::Scanner {
  BlueStore* bluestore;
public:
  Scanner(BlueStore* bluestore)
  :bluestore(bluestore) {}

  void write_lookaround(
    BlueStore::ExtentMap* extent_map,
    uint32_t offset, uint32_t length,
    uint32_t left_limit, uint32_t right_limit,
    Estimator* estimator);
  class Scan;
};

#endif
