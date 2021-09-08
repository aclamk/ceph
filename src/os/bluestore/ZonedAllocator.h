// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

// 
// A simple allocator that just hands out space from the next empty zone.  This
// is temporary, just to get the simplest append-only write workload to work.
//
// Copyright (C) 2020 Abutalib Aghayev
//

#ifndef CEPH_OS_BLUESTORE_ZONEDALLOCATOR_H
#define CEPH_OS_BLUESTORE_ZONEDALLOCATOR_H

#include <mutex>

#include "Allocator.h"
#include "common/ceph_mutex.h"
#include "include/btree_map.h"
#include "include/interval_set.h"
#include "include/mempool.h"
#include "bluestore_types.h"
#include "zoned_types.h"

class ZonedAllocator : public Allocator {
  CephContext* cct;

  // Currently only one thread at a time calls into ZonedAllocator due to
  // atomic_alloc_and_submit_lock in BlueStore.cc, but we do locking anyway
  // because eventually ZONE_APPEND support will land and
  // atomic_alloc_and_submit_lock will be removed.
  ceph::mutex lock = ceph::make_mutex("ZonedAllocator::lock");

  std::atomic<int64_t> num_free;     ///< total bytes in freelist
  uint64_t size;
  uint64_t conventional_size, sequential_size;
  uint64_t block_size;
  uint64_t zone_size;
  uint64_t first_seq_zone_num;
  uint64_t starting_zone_num;
  uint64_t num_zones;
  std::vector<zone_state_t> zone_states;
  std::set<uint64_t> zones_to_clean;
  std::atomic<int64_t> num_zones_to_clean;

  ceph::mutex *cleaner_lock = nullptr;
  ceph::condition_variable *cleaner_cond = nullptr;

  inline uint64_t get_offset(uint64_t zone_num) const {
    return zone_num * zone_size + get_write_pointer(zone_num);
  }

  inline uint64_t get_write_pointer(uint64_t zone_num) const {
    return zone_states[zone_num].get_write_pointer();
  }

  inline uint64_t get_remaining_space(uint64_t zone_num) const {
    return zone_size - get_write_pointer(zone_num);
  }

  inline void increment_write_pointer(uint64_t zone_num, uint64_t want_size) {
    zone_states[zone_num].increment_write_pointer(want_size);
  }

  inline void increment_num_dead_bytes(uint64_t zone_num, uint64_t length) {
    zone_states[zone_num].increment_num_dead_bytes(length);
  }

  inline bool fits(uint64_t want_size, uint64_t zone_num) const {
    return want_size <= get_remaining_space(zone_num);
  }

public:
  ZonedAllocator(CephContext* cct, int64_t size, int64_t block_size,
		 int64_t _zone_size,
		 int64_t _first_sequential_zone,
		 std::string_view name);
  ~ZonedAllocator() override;

  const char *get_type() const override {
    return "zoned";
  }

  uint64_t get_dead_bytes(uint32_t zone) {
    return zone_states[zone].num_dead_bytes;
  }

  int64_t allocate(
    uint64_t want_size, uint64_t alloc_unit, uint64_t max_alloc_size,
    int64_t hint, PExtentVector *extents) override;

  void release(const interval_set<uint64_t>& release_set) override;

  uint64_t get_free() override;

  void dump() override;
  void dump(std::function<void(uint64_t offset,
                               uint64_t length)> notify) override;

  const std::set<uint64_t> *get_zones_to_clean(void);
  void mark_zones_to_clean_free(void);

  void init_from_zone_pointers(
    std::vector<zone_state_t> _zone_states,
    ceph::mutex *_cleaner_lock,
    ceph::condition_variable *_cleaner_cond);
  void init_add_free(uint64_t offset, uint64_t length) override {}
  void init_rm_free(uint64_t offset, uint64_t length) override {}

  void shutdown() override;

private:
  bool low_on_space(void);
  void find_zones_to_clean(void);
};

#endif
