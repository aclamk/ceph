// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
  *
 * Copyright (C) 2015 XSky <haomai@xsky.com>
 *
 * Author: Haomai Wang <haomaiwang@gmail.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef CEPH_COMMON_ALIGN_H
#define CEPH_COMMON_ALIGN_H

#include <type_traits>

template <typename T, typename U>
inline constexpr T align_up(T v, U align) {
  static_assert(std::is_convertible_v<U, T>);
  return (v + align - 1) & ~(align - 1);
}

template <typename T, typename U>
inline constexpr T align_down(T v, U align) {
  static_assert(std::is_convertible_v<U, T>);
  return v & ~(static_cast<T>(align) - 1);
}

#endif /* CEPH_COMMON_ALIGN_H */
