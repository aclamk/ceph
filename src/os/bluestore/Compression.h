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

class recompression_scanner {
public:
  BlueStore* bluestore;
  recompression_scanner(BlueStore* bluestore)
  :bluestore(bluestore) {}

  class on_write;
  on_write* on_write_start(
      BlueStore::ExtentMap* extent_map,
      uint32_t offset, uint32_t length,
      uint32_t left_limit, uint32_t right_limit,
      interval_set<uint32_t>& extra_rewrites);
  void on_write_done(on_write*);
};
#endif