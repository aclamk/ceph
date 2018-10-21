// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */
#ifndef CEPH_ENCODING_H
#define CEPH_ENCODING_H

#include <set>
#include <map>
#include <deque>
#include <vector>
#include <string>
#include <string_view>
#include <tuple>
#include <boost/optional/optional_io.hpp>
#include <boost/tuple/tuple.hpp>

#include "include/unordered_map.h"
#include "include/unordered_set.h"
#include "common/ceph_time.h"

#include "include/int_types.h"

#include "common/convenience.h"

#include "byteorder.h"
#include "buffer.h"

// pull in the new-style encoding so that we get the denc_traits<> definition.
#include "denc.h"

#include "assert.h"

using namespace ceph;

namespace ceph {

/*
 * Notes on feature encoding:
 *
 * - The default encode() methods have a features argument with a default parameter
 *   (which goes to zero).
 * - Normal classes will use WRITE_CLASS_ENCODER, with that features=0 default.
 * - Classes that _require_ features will use WRITE_CLASS_ENCODER_FEATURES, which
 *   does not define the default.  Any caller must explicitly pass it in.
 * - STL container macros have two encode variants: one with a features arg, and one
 *   without.
 *
 * The result:
 * - A feature encode() method will fail to compile if a value is not
 *   passed in.
 * - The feature varianet of the STL templates will be used when the feature arg is
 *   provided.  It will be passed through to any template arg types, but it will be
 *   ignored when not needed.
 */

// --------------------------------------
// base types

  class encode_type {};

  //calculate size stage
  class replacement;
  class encode_size;

  class encode_size : public encode_type
  {
  public:
    class filler {
    public:
      void copy_in(const unsigned len, const char* const src) {};
    };
    using contiguous_filler = filler;
    size_t encode{0};
    filler append_hole(size_t v) {encode+=v; return filler();}
    size_t length() {return 0;}
    void append(const char* src, size_t len) {encode+=len;};
    void append(const bufferptr& ptr) {/*no code*/};
    void append(const bufferlist& bl) {/*no code*/};
    template<std::size_t N> void append(const char (&s)[N]) {
    	encode+=N;//append(s, N);
    }
    void append(std::string s) {
    	append(s.data(), s.length());
    }
    inline operator replacement& ();// {replacement(*this);}
    //template <class T> operator std::enable_if_t<std::is_same_v<T, replacement>, replacement> () {};
    //replacement ();// {replacement(*this);}
  };

  class replacement1;
  class encode_helper : public encode_type
  {
  public:
    buffer::list::contiguous_filler at{nullptr};
    buffer::list* bl;

    encode_helper() {}
    ~encode_helper() {};
    class filler {
      char* pos;
      public:
        filler(char* pos) : pos(pos) {};
        void copy_in(const unsigned len, const char* const src)
        {
          memcpy(pos, src, len);
          pos += len;
        }
      };
    using contiguous_filler = filler;
    filler append_hole(size_t v) {return filler{at.pos}; at.pos+=v;}
    size_t length() {return (size_t)at.pos;}
    void append(const char* src, size_t len) {at.copy_in(len, src);}
    void append(const bufferptr& ptr) {this->bl->insert(ptr, at);  /*implement me*/};
    void append(const bufferlist& bl) {this->bl->insert(bl, at);  /*implement me*/};
    template<std::size_t N> void append(const char (&s)[N]) {
    	append(s, N);
    }
    void append(std::string s) {
    	append(s.data(), s.length());
    }
    inline operator replacement1& ();
  };
  /*
  template<class A, class B, class K,
  	 typename a_traits=denc_traits<A>, typename b_traits=denc_traits<B>>
  inline std::enable_if_t<!a_traits::supported || !b_traits::supported || std::is_base_of_v<encode_type, K> >

      static void bound_encode(const T& v, size_t& p, uint64_t f=0) {	\
      v.bound_encode(p);						\
    }
  */



  template <class R> class WR {
      public:
        WR(bufferlist& bl, const R& obj) {
          {
            encode_size s;
            encode_helper h;
            obj.encode(s);
            h.bl = &bl;
            h.at = bl.append_hole(s.encode);
            obj.encode(h);
            return;
          }
        }
      };
  template <class R, class S> class DUMMY {
  public:
    DUMMY(R &bl, S) {};
  };


  template <typename T>
  struct has_encode_method_features
  {
      struct dummy { /* something */ };

      template <typename C, typename P>
      static auto test(P * p) -> decltype(std::declval<C>().encode(std::declval<encode_size&>(), 0), std::true_type());

      template <typename, typename>
      static std::false_type test(...);

      //typedef decltype(test<T, dummy>(nullptr)) type;
      static const bool value = std::is_same<std::true_type, decltype(test<T,dummy>(nullptr))>::value;
  };

  template <typename T>
  struct has_encode_method
  {
      struct dummy { /* something */ };

      template <typename C, typename P>
      static auto test(P * p) -> decltype(std::declval<C>().encode(std::declval<encode_size&>()), std::true_type());

      template <typename, typename>
      static std::false_type test(...);

      //typedef decltype(test<T, dummy>(nullptr)) type;
      static const bool value = std::is_same<std::true_type, decltype(test<T,dummy>(nullptr))>::value;
  };

  template <typename T>
  struct has_encode_method_bufferlist
  {
      struct dummy { /* something */ };

      template <typename C, typename P>
      static auto test(P * p) -> decltype(std::declval<C>().encode(std::declval<bufferlist&>()), std::true_type());

      template <typename, typename>
      static std::false_type test(...);

      //typedef decltype(test<T, dummy>(nullptr)) type;
      static const bool value = std::is_same<std::true_type, decltype(test<T,dummy>(nullptr))>::value;
  };

  template <typename T>
  struct exists_encode_function_features
    {
        struct dummy { /* something */ };

        template <typename C, typename P>
        static auto test(P * p) -> decltype(encode(std::declval<C&>(),std::declval<encode_size&>(), 0), std::true_type());

        template <typename, typename>
        static std::false_type test(...);

        //typedef decltype(test<T, dummy>(nullptr)) type;
        static const bool value = std::is_same<std::true_type, decltype(test<T,dummy>(nullptr))>::value;
    };



#if 1
  class replacement : public encode_size {
    //replacement(encode_size& bl) : bl(bl) {}
  public:
      //replacement(encode_size bl) : bl(bl) {}
    //operator encode_size&() { return bl;};
    //encode_size& bl;
	  void advance(size_t p) {encode += p;}
  };

  inline encode_size::operator replacement& (){return *((replacement*)this);};


  template <class T> inline std::enable_if_t<denc_traits<T>::supported && !has_encode_method<T>::value>
  encode(const T& t, replacement& r)
  {
	  size_t p = 0;
	  denc_traits<T>::bound_encode(t, p);
	  //r.bl.encode += p;
	  r.advance(p);
  }
  template <class T> inline std::enable_if_t<denc_traits<T>::supported && !has_encode_method<T>::value >
  encode(const T& t, replacement& r, uint64_t f)
  {
	  size_t p = 0;
	  denc_traits<T>::bound_encode(t, p);
	  //r.bl.encode += p;
	  //r.advance(p);
	  r.encode += p;
  }

  template <class T> inline std::enable_if_t<!denc_traits<T>::supported && has_encode_method_bufferlist<T>::value>
  encode(const T& t, replacement& r)
  {
	  size_t p = 0;
	  //denc_traits<T>::bound_encode(t, p);
	  //r.bl.encode += p;
	  //r.advance(p);
	  //t.encode(r.bl);
  }

  class replacement1 : public encode_helper {
  public:
    //replacement1(encode_helper&) {}
  };
  inline encode_helper::operator replacement1& (){return *((replacement1*)this);};

#if 0
  const T& o,
   bufferlist& bl,
   uint64_t features_unused=0)
 {
   size_t len = 0;
   traits::bound_encode(o, len);
   auto a = bl.get_contiguous_appender(len);
   traits::encode(o, a);
 }
#endif

  template <class T> inline std::enable_if_t<denc_traits<T>::supported>
  encode(const T& t, replacement1& r)
  {
	  bufferlist bl;
	  size_t len = 0;
	  denc_traits<T>::bound_encode(t, len);
	  auto a = bl.get_contiguous_appender(len);
	  denc_traits<T>::encode(t, a);

	  //t.encode(*(encode_helper*)&r);
	  r.append(bl);
  }

  template <class T> inline std::enable_if_t<denc_traits<T>::supported>
  encode(const T& t, replacement1& r, uint64_t f)
  {
	  bufferlist bl;
	  size_t len = 0;
	  denc_traits<T>::bound_encode(t, len);
	  auto a = bl.get_contiguous_appender(len);
	  denc_traits<T>::encode(t, a);

	  //t.encode(*(encode_helper*)&r);
	  r.append(bl);
  }

  template <class T> inline std::enable_if_t<!denc_traits<T>::supported && has_encode_method_bufferlist<T>::value>
  encode(const T& t, replacement1& r)
  {
	  bufferlist bl;
	  t.encode(bl);
	  //size_t len = 0;
	  ///denc_traits<T>::bound_encode(t, len);
	  ///auto a = bl.get_contiguous_appender(len);
	  ///denc_traits<T>::encode(t, a);

	  //t.encode(*(encode_helper*)&r);
	  r.append(bl);
  }
#endif
#if 0
  template <class T> inline std::enable_if_t<denc_traits<T>::supported>
  encode(const T& t, encode_size& bl)
  {
	  size_t p;
	  denc_traits<T>::bound_encode(t, p);
	  bl.encode += p;
  }
#endif


#define encode_two_phase_trampoline \
  { typename std::conditional<std::is_same_v<T, bufferlist>, WR<decltype(*this)> ,DUMMY<T, decltype(*this)> >::type wr(bl,*this); \
    if (std::is_same_v<T, bufferlist>) return; }



#define enable_if_enctype(K) std::enable_if_t< std::is_same_v<K, ::ceph::bufferlist> || \
                                 std::is_same_v<K, encode_size> || \
                                 std::is_same_v<K, encode_helper> >


template<class T>
inline void encode_raw(const T& t, bufferlist& bl)
{
  bl.append((char*)&t, sizeof(t));
}
template<class T>
inline void encode_raw(const T& t, encode_helper& bl)
{
  bl.at.copy_in(sizeof(t),(char*)&t);
}
template<class T>
inline void encode_raw(const T& t, encode_size& bl)
{
  bl.encode += sizeof(t);
}
template<class T>
inline void decode_raw(T& t, bufferlist::const_iterator &p)
{
  p.copy(sizeof(t), (char*)&t);
}

#define WRITE_RAW_ENCODER(type)						\
  inline void encode(const type &v, ::ceph::bufferlist& bl, uint64_t features=0) { ::ceph::encode_raw(v, bl); } \
  inline void encode(const type &v, encode_size& bl, uint64_t features=0) { ::ceph::encode_raw(v, bl); } \
  inline void encode(const type &v, encode_helper& bl, uint64_t features=0) { ::ceph::encode_raw(v, bl); } \
  inline void decode(type &v, ::ceph::bufferlist::const_iterator& p) { ::ceph::decode_raw(v, p); }

WRITE_RAW_ENCODER(__u8)
#ifndef _CHAR_IS_SIGNED
WRITE_RAW_ENCODER(__s8)
#endif
WRITE_RAW_ENCODER(char)
WRITE_RAW_ENCODER(ceph_le64)
WRITE_RAW_ENCODER(ceph_le32)
WRITE_RAW_ENCODER(ceph_le16)

// FIXME: we need to choose some portable floating point encoding here
WRITE_RAW_ENCODER(float)
WRITE_RAW_ENCODER(double)

inline void encode(const bool &v, bufferlist& bl) {
  __u8 vv = v;
  encode_raw(vv, bl);
}
inline void encode(const bool &v, encode_size& bl) {
  __u8 vv = v;
  encode_raw(vv, bl);
}
inline void encode(const bool &v, encode_helper& bl) {
  __u8 vv = v;
  encode_raw(vv, bl);
}
inline void decode(bool &v, bufferlist::const_iterator& p) {
  __u8 vv;
  decode_raw(vv, p);
  v = vv;
}


// -----------------------------------
// int types

#define WRITE_INTTYPE_ENCODER(type, etype)				\
  inline void encode(type v, ::ceph::bufferlist& bl, uint64_t features=0) { \
    ceph_##etype e;					                \
    e = v;                                                              \
    ::ceph::encode_raw(e, bl);						\
  }									\
  inline void encode(type v, encode_helper& bl, uint64_t features=0) {  \
    ceph_##etype e;                                                     \
    e = v;                                                              \
    ::ceph::encode_raw(e, bl);                                          \
  }                                                                     \
  inline void encode(type v, encode_size& bl, uint64_t features=0) {    \
    ceph_##etype e;                                                     \
    e = v;                                                              \
    ::ceph::encode_raw(e, bl);                                          \
  }                                                                     \
  inline void decode(type &v, ::ceph::bufferlist::const_iterator& p) {	\
    ceph_##etype e;							\
    ::ceph::decode_raw(e, p);						\
    v = e;								\
  }

WRITE_INTTYPE_ENCODER(uint64_t, le64)
WRITE_INTTYPE_ENCODER(int64_t, le64)
WRITE_INTTYPE_ENCODER(uint32_t, le32)
WRITE_INTTYPE_ENCODER(int32_t, le32)
WRITE_INTTYPE_ENCODER(uint16_t, le16)
WRITE_INTTYPE_ENCODER(int16_t, le16)

// see denc.h for ENCODE_DUMP_PATH discussion and definition.
#ifdef ENCODE_DUMP_PATH
# define ENCODE_DUMP_PRE()			\
  unsigned pre_off = bl.length()
# define ENCODE_DUMP_POST(cl)						\
  do {									\
    static int i = 0;							\
    i++;								\
    int bits = 0;							\
    for (unsigned t = i; t; bits++)					\
      t &= t - 1;							\
    if (bits > 2)							\
      break;								\
    char fn[PATH_MAX];							\
    snprintf(fn, sizeof(fn), ENCODE_STRINGIFY(ENCODE_DUMP_PATH) "/%s__%d.%x", #cl, getpid(), i++); \
    int fd = ::open(fn, O_WRONLY|O_TRUNC|O_CREAT, 0644);		\
    if (fd >= 0) {							\
      ::ceph::bufferlist sub;						\
      sub.substr_of(bl, pre_off, bl.length() - pre_off);		\
      sub.write_fd(fd);							\
      ::close(fd);							\
    }									\
  } while (0)
#else
# define ENCODE_DUMP_PRE()
# define ENCODE_DUMP_POST(cl)
#endif


#define WRITE_CLASS_ENCODER(cl)						\
  inline void encode(const cl &c, ::ceph::bufferlist &bl, uint64_t features=0) { \
    ENCODE_DUMP_PRE(); c.encode(bl); ENCODE_DUMP_POST(cl); }		\
  template <class TT> \
	inline std::enable_if_t< has_encode_method<cl>::value && std::is_same_v<encode_size, TT> > \
    encode(const cl &c, TT &bl, uint64_t features=0) { \
    ENCODE_DUMP_PRE(); c.encode(bl); ENCODE_DUMP_POST(cl); }            \
  template <class TT> \
	inline std::enable_if_t< has_encode_method<cl>::value && std::is_same_v<encode_helper, TT> > \
    encode(const cl &c, TT &bl, uint64_t features=0) { \
    ENCODE_DUMP_PRE(); c.encode(bl); ENCODE_DUMP_POST(cl); }            \
  inline void decode(cl &c, ::ceph::bufferlist::const_iterator &p) { c.decode(p); }

#define WRITE_CLASS_MEMBER_ENCODER(cl)					\
  inline void encode(const cl &c, ::ceph::bufferlist &bl) const {	\
    ENCODE_DUMP_PRE(); c.encode(bl); ENCODE_DUMP_POST(cl); }		\
  inline void decode(cl &c, ::ceph::bufferlist::const_iterator &p) { c.decode(p); }

#define WRITE_CLASS_ENCODER_FEATURES(cl)				\
  inline void encode(const cl &c, ::ceph::bufferlist &bl, uint64_t features) { \
    ENCODE_DUMP_PRE(); c.encode(bl, features); ENCODE_DUMP_POST(cl); }	\
  template <class TT> \
	inline std::enable_if_t< has_encode_method_features<cl>::value && std::is_same_v<encode_size, TT> > \
    encode(const cl &c, TT &bl, uint64_t features) { \
    ENCODE_DUMP_PRE(); c.encode(bl, features); ENCODE_DUMP_POST(cl); }	\
  template <class TT> \
	inline std::enable_if_t< has_encode_method_features<cl>::value && std::is_same_v<encode_helper, TT> > \
    encode(const cl &c, TT &bl, uint64_t features) { \
    ENCODE_DUMP_PRE(); c.encode(bl, features); ENCODE_DUMP_POST(cl); }	\
  inline void decode(cl &c, ::ceph::bufferlist::const_iterator &p) { c.decode(p); }

/*
 *
 inline void encode(const cl &c, encode_size &bl, uint64_t features) { \
  ENCODE_DUMP_PRE(); c.encode(bl, features); ENCODE_DUMP_POST(cl); }	\
*/

#define WRITE_CLASS_ENCODER_OPTIONAL_FEATURES(cl)				\
  inline void encode(const cl &c, ::ceph::bufferlist &bl, uint64_t features = 0) { \
    ENCODE_DUMP_PRE(); c.encode(bl, features); ENCODE_DUMP_POST(cl); }	\
  inline void decode(cl &c, ::ceph::bufferlist::const_iterator &p) { c.decode(p); }


// string
//template <class TT>
inline void encode(std::string_view s, bufferlist& bl, uint64_t features=0)
{
  __u32 len = s.length();
  encode(len, bl);
  if (len)
    bl.append(s.data(), len);
}
inline void encode(std::string_view s, encode_size& bl, uint64_t features=0)
{
  __u32 len = s.length();
  encode(len, bl);
  if (len)
    bl.append(s.data(), len);
}
inline void encode(std::string_view s, encode_helper& bl, uint64_t features=0)
{
  __u32 len = s.length();
  encode(len, bl);
  if (len)
    bl.append(s.data(), len);
}

inline void encode(const std::string& s, bufferlist& bl, uint64_t features=0)
{
  return encode(std::string_view(s), bl, features);
}
inline void encode(const std::string& s, encode_size& bl, uint64_t features=0)
{
  return encode(std::string_view(s), bl, features);
}
inline void encode(const std::string& s, encode_helper& bl, uint64_t features=0)
{
  return encode(std::string_view(s), bl, features);
}
inline void decode(std::string& s, bufferlist::const_iterator& p)
{
  __u32 len;
  decode(len, p);
  s.clear();
  p.copy(len, s);
}
template <class K>
inline void encode_nohead(std::string_view s, K& bl)
{
  bl.append(s.data(), s.length());
}
template <class K> inline std::enable_if_t<std::is_base_of_v<encode_type, K> >
encode_nohead(const std::string& s, K& bl)
{
  encode_nohead(std::string_view(s), bl);
}
inline void decode_nohead(int len, std::string& s, bufferlist::const_iterator& p)
{
  s.clear();
  p.copy(len, s);
}

// const char* (encode only, string compatible)
inline void encode(const char *s, bufferlist& bl) 
{
  encode(std::string_view(s, strlen(s)), bl);
}


// -----------------------------
// buffers

// bufferptr (encapsulated)
inline void encode(const buffer::ptr& bp, bufferlist& bl) 
{
  __u32 len = bp.length();
  encode(len, bl);
  if (len)
    bl.append(bp);
}
inline void encode(const buffer::ptr& bp, encode_size& bl)
{
  __u32 len = bp.length();
  encode(len, bl);
  if (len)
    bl.append(bp);
}
inline void encode(const buffer::ptr& bp, encode_helper& bl)
{
  __u32 len = bp.length();
  encode(len, bl);
  if (len)
    bl.append(bp);
}
inline void decode(buffer::ptr& bp, bufferlist::const_iterator& p)
{
  __u32 len;
  decode(len, p);

  bufferlist s;
  p.copy(len, s);

  if (len) {
    if (s.get_num_buffers() == 1)
      bp = s.front();
    else
      bp = buffer::copy(s.c_str(), s.length());
  }
}

// bufferlist (encapsulated)
inline void encode(const bufferlist& s, bufferlist& bl)
{
  __u32 len = s.length();
  encode(len, bl);
  bl.append(s);
}
inline void encode(const bufferlist& s, encode_size& bl)
{
  __u32 len = s.length();
  encode(len, bl);
  bl.append(s);
}
inline void encode(const bufferlist& s, encode_helper& bl)
{
  __u32 len = s.length();
  encode(len, bl);
  bl.append(s);
}
inline void encode_destructively(bufferlist& s, bufferlist& bl)
{
  __u32 len = s.length();
  encode(len, bl);
  bl.claim_append(s);
}
inline void decode(bufferlist& s, bufferlist::const_iterator& p)
{
  __u32 len;
  decode(len, p);
  s.clear();
  p.copy(len, s);
}
template <class K> inline std::enable_if_t<std::is_base_of_v<encode_type, K> >
encode_nohead(const bufferlist& s, K& bl)
{
  bl.append(s);
}
inline void decode_nohead(int len, bufferlist& s, bufferlist::const_iterator& p)
{
  s.clear();
  p.copy(len, s);
}

// Time, since the templates are defined in std::chrono

template<typename Clock, typename Duration, class K,
         typename std::enable_if_t<converts_to_timespec_v<Clock>>* = nullptr>
void encode(const std::chrono::time_point<Clock, Duration>& t,
	    K &bl) {
  auto ts = Clock::to_timespec(t);
  // A 32 bit count of seconds causes me vast unhappiness.
  uint32_t s = ts.tv_sec;
  uint32_t ns = ts.tv_nsec;
  encode(s, bl);
  encode(ns, bl);
}

template<typename Clock, typename Duration,
         typename std::enable_if_t<converts_to_timespec_v<Clock>>* = nullptr>
void decode(std::chrono::time_point<Clock, Duration>& t,
	    bufferlist::const_iterator& p) {
  uint32_t s;
  uint32_t ns;
  decode(s, p);
  decode(ns, p);
  struct timespec ts = {
    static_cast<time_t>(s),
    static_cast<long int>(ns)};

  t = Clock::from_timespec(ts);
}

template<typename Rep, typename Period, class K,
         typename std::enable_if_t<std::is_integral_v<Rep>>* = nullptr>
void encode(const std::chrono::duration<Rep, Period>& d,
	    K &bl) {
  using namespace std::chrono;
  uint32_t s = duration_cast<seconds>(d).count();
  uint32_t ns = (duration_cast<nanoseconds>(d) % seconds(1)).count();
  encode(s, bl);
  encode(ns, bl);
}

template<typename Rep, typename Period,
         typename std::enable_if_t<std::is_integral_v<Rep>>* = nullptr>
void decode(std::chrono::duration<Rep, Period>& d,
	    bufferlist::const_iterator& p) {
  uint32_t s;
  uint32_t ns;
  decode(s, p);
  decode(ns, p);
  d = std::chrono::seconds(s) + std::chrono::nanoseconds(ns);
}

// -----------------------------
// STL container types

template<typename T, class K>
inline std::enable_if_t<!denc_traits<T>::supported || std::is_base_of_v<encode_type, K> >
encode(const boost::optional<T> &p, K &bl);
template<typename T>
inline void decode(boost::optional<T> &p, bufferlist::const_iterator &bp);
template<class A, class B, class C, class K>
inline void encode(const boost::tuple<A, B, C> &t, K& bl);
template<class A, class B, class C>
inline void decode(boost::tuple<A, B, C> &t, bufferlist::const_iterator &bp);
template<class A, class B, class K,
	 typename a_traits=denc_traits<A>, typename b_traits=denc_traits<B>>
inline std::enable_if_t<!a_traits::supported || !b_traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::pair<A,B> &p, K &bl, uint64_t features);
template<class A, class B, class K,
	 typename a_traits=denc_traits<A>, typename b_traits=denc_traits<B>>
inline std::enable_if_t<!a_traits::supported || !b_traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::pair<A,B> &p, K &bl);
template<class A, class B,
	 typename a_traits=denc_traits<A>, typename b_traits=denc_traits<B>>
inline std::enable_if_t<!a_traits::supported || !b_traits::supported>
decode(std::pair<A,B> &pa, bufferlist::const_iterator &p);
template<class T, class K, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::list<T, Alloc>& ls, K& bl);
template<class T, class K, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::list<T,Alloc>& ls, K& bl, uint64_t features);
template<class T, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
decode(std::list<T,Alloc>& ls, bufferlist::const_iterator& p);
template<class T, class Alloc>
inline void encode(const std::list<std::shared_ptr<T>, Alloc>& ls,
		   bufferlist& bl);
template<class T, class Alloc>
inline void encode(const std::list<std::shared_ptr<T>, Alloc>& ls,
		   bufferlist& bl, uint64_t features);
template<class T, class Alloc>
inline void decode(std::list<std::shared_ptr<T>, Alloc>& ls,
		   bufferlist::const_iterator& p);
template<class T, class K, class Comp, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::set<T,Comp,Alloc>& s, K& bl);
template<class T, class Comp, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
decode(std::set<T,Comp,Alloc>& s, bufferlist::const_iterator& p);
template<class T, class K, class Comp, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
encode_nohead(const std::set<T,Comp,Alloc>& s, K& bl);
template<class T, class Comp, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
decode_nohead(int len, std::set<T,Comp,Alloc>& s, bufferlist::iterator& p);
template<class T, class Comp, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
encode(const boost::container::flat_set<T, Comp, Alloc>& s, bufferlist& bl);
template<class T, class Comp, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
decode(boost::container::flat_set<T, Comp, Alloc>& s, bufferlist::const_iterator& p);
template<class T, class Comp, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
encode_nohead(const boost::container::flat_set<T, Comp, Alloc>& s,
	      bufferlist& bl);
template<class T, class Comp, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
decode_nohead(int len, boost::container::flat_set<T, Comp, Alloc>& s,
	      bufferlist::iterator& p);
template<class T, class Comp, class Alloc>
inline void encode(const std::multiset<T,Comp,Alloc>& s, bufferlist& bl);
template<class T, class Comp, class Alloc>
inline void decode(std::multiset<T,Comp,Alloc>& s, bufferlist::const_iterator& p);
template<class T, class Alloc, class K, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::vector<T,Alloc>& v, K& bl, uint64_t features);
template<class T, class Alloc, class K, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::vector<T,Alloc>& v, K& bl);
template<class T, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
decode(std::vector<T,Alloc>& v, bufferlist::const_iterator& p);
template<class T, class Alloc, class K, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
encode_nohead(const std::vector<T,Alloc>& v, K& bl);
template<class T, class Alloc, typename traits=denc_traits<T>>
inline std::enable_if_t<!traits::supported>
decode_nohead(int len, std::vector<T,Alloc>& v, bufferlist::const_iterator& p);
template<class T,class Alloc, class K>
inline void encode(const std::vector<std::shared_ptr<T>,Alloc>& v,
		   K& bl,
		   uint64_t features);
template<class T, class Alloc, class K>
inline void encode(const std::vector<std::shared_ptr<T>,Alloc>& v,
		   K& bl);
template<class T, class Alloc>
inline void decode(std::vector<std::shared_ptr<T>,Alloc>& v,
		   bufferlist::const_iterator& p);
template<class T, class U, class K, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported ||	!u_traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::map<T,U,Comp,Alloc>& m, K& bl);
template<class T, class U, class K, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported || std::is_base_of_v<encode_type, K> >
encode(const std::map<T,U,Comp,Alloc>& m, K& bl, uint64_t features);
template<class T, class U, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
decode(std::map<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p);
template<class T, class U, class Comp, class Alloc>
inline void decode_noclear(std::map<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p);
template<class T, class U, class Comp, class Alloc, class K,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported || std::is_base_of_v<encode_type, K> >
encode_nohead(const std::map<T,U,Comp,Alloc>& m, K& bl);
template<class T, class U, class Comp, class Alloc, class K,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported || std::is_base_of_v<encode_type, K> >
encode_nohead(const std::map<T,U,Comp,Alloc>& m, K& bl, uint64_t features);
template<class T, class U, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
decode_nohead(int n, std::map<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p);
template<class T, class U, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
  inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
encode(const boost::container::flat_map<T,U,Comp,Alloc>& m, bufferlist& bl);
template<class T, class U, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
encode(const boost::container::flat_map<T,U,Comp,Alloc>& m, bufferlist& bl,
       uint64_t features);
template<class T, class U, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
decode(boost::container::flat_map<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p);
template<class T, class U, class Comp, class Alloc>
inline void decode_noclear(boost::container::flat_map<T,U,Comp,Alloc>& m,
			   bufferlist::const_iterator& p);
template<class T, class U, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
encode_nohead(const boost::container::flat_map<T,U,Comp,Alloc>& m,
	      bufferlist& bl);
template<class T, class U, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
encode_nohead(const boost::container::flat_map<T,U,Comp,Alloc>& m,
	      bufferlist& bl, uint64_t features);
template<class T, class U, class Comp, class Alloc,
	 typename t_traits=denc_traits<T>, typename u_traits=denc_traits<U>>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
decode_nohead(int n, boost::container::flat_map<T,U,Comp,Alloc>& m,
	      bufferlist::const_iterator& p);
template<class T, class K, class U, class Comp, class Alloc>
inline void encode(const std::multimap<T,U,Comp,Alloc>& m, K& bl);
template<class T, class U, class Comp, class Alloc>
inline void decode(std::multimap<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p);
template<class T, class U, class Hash, class Pred, class Alloc, class K>
inline void encode(const unordered_map<T,U,Hash,Pred,Alloc>& m, K& bl,
		   uint64_t features);
template<class T, class U, class Hash, class Pred, class Alloc, class K>
inline void encode(const unordered_map<T,U,Hash,Pred,Alloc>& m, K& bl);
template<class T, class U, class Hash, class Pred, class Alloc>
inline void decode(unordered_map<T,U,Hash,Pred,Alloc>& m, bufferlist::const_iterator& p);
template<class T, class K, class Hash, class Pred, class Alloc>
inline void encode(const ceph::unordered_set<T,Hash,Pred,Alloc>& m, K& bl);
template<class T, class Hash, class Pred, class Alloc>
inline void decode(ceph::unordered_set<T,Hash,Pred,Alloc>& m, bufferlist::const_iterator& p);
template<class T, class Alloc>
inline void encode(const std::deque<T,Alloc>& ls, bufferlist& bl, uint64_t features);
template<class T, class Alloc>
inline void encode(const std::deque<T,Alloc>& ls, bufferlist& bl);
template<class T, class Alloc>
inline void decode(std::deque<T,Alloc>& ls, bufferlist::const_iterator& p);
template<class T, size_t N, typename traits = denc_traits<T>>
inline std::enable_if_t<!traits::supported>
encode(const std::array<T, N>& v, bufferlist& bl, uint64_t features);
template<class T, size_t N, typename traits = denc_traits<T>>
inline std::enable_if_t<!traits::supported>
encode(const std::array<T, N>& v, bufferlist& bl);
template<class T, size_t N, typename traits = denc_traits<T>>
inline std::enable_if_t<!traits::supported>
decode(std::array<T, N>& v, bufferlist::const_iterator& p);

// full bl decoder
template<class T>
inline void decode(T &o, const bufferlist& bl)
{
  auto p = bl.begin();
  decode(o, p);
  ceph_assert(p.end());
}

// boost optional
template<typename T, class K>
inline std::enable_if_t<!denc_traits<T>::supported || std::is_base_of_v<encode_type, K> >
encode(const boost::optional<T> &p, K &bl)
{
  __u8 present = static_cast<bool>(p);
  encode(present, bl);
  if (p)
    encode(p.get(), bl);
}

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
template<typename T>
inline void decode(boost::optional<T> &p, bufferlist::const_iterator &bp)
{
  __u8 present;
  decode(present, bp);
  if (present) {
    p = T{};
    decode(p.get(), bp);
  } else {
    p = boost::none;
  }
}
#pragma GCC diagnostic pop
#pragma GCC diagnostic warning "-Wpragmas"

// std::tuple
template<typename... Ts>
inline void encode(const std::tuple<Ts...> &t, bufferlist& bl)
{
  ceph::for_each(t, [&bl](const auto& e) {
      encode(e, bl);
    });
}
template<typename... Ts>
inline void decode(std::tuple<Ts...> &t, bufferlist::const_iterator &bp)
{
  ceph::for_each(t, [&bp](auto& e) {
      decode(e, bp);
    });
}

//triple boost::tuple
template<class A, class B, class C, class K>
inline void encode(const boost::tuple<A, B, C> &t, K& bl)
{
  encode(boost::get<0>(t), bl);
  encode(boost::get<1>(t), bl);
  encode(boost::get<2>(t), bl);
}
template<class A, class B, class C>
inline void decode(boost::tuple<A, B, C> &t, bufferlist::const_iterator &bp)
{
  decode(boost::get<0>(t), bp);
  decode(boost::get<1>(t), bp);
  decode(boost::get<2>(t), bp);
}

// std::pair<A,B>
template<class A, class B, class K,
	 typename a_traits, typename b_traits>
inline std::enable_if_t<!a_traits::supported || !b_traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::pair<A,B> &p, K &bl, uint64_t features)
{
  encode(p.first, bl, features);
  encode(p.second, bl, features);
}
template<class A, class B, class K,
	 typename a_traits, typename b_traits>
inline std::enable_if_t<!a_traits::supported || !b_traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::pair<A,B> &p, K &bl)
{
  encode(p.first, bl);
  encode(p.second, bl);
}
template<class A, class B, typename a_traits, typename b_traits>
inline std::enable_if_t<!a_traits::supported ||
			!b_traits::supported>
  decode(std::pair<A,B> &pa, bufferlist::const_iterator &p)
{
  decode(pa.first, p);
  decode(pa.second, p);
}

// std::list<T>
template<class T, class K, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::list<T, Alloc>& ls, K& bl)
{
  __u32 n = (__u32)(ls.size());  // c++11 std::list::size() is O(1)
  encode(n, bl);
  for (auto p = ls.begin(); p != ls.end(); ++p)
    encode(*p, bl);
}
template<class T, class K, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::list<T,Alloc>& ls, K& bl, uint64_t features)
{
  // should i pre- or post- count?
#if AK_DISABLED
  if (!ls.empty()) {
    unsigned pos = bl.length();
    unsigned n = 0;
    encode(n, bl);
    for (auto p = ls.begin(); p != ls.end(); ++p) {
      n++;
      encode(*p, bl, features);
    }
    ceph_le32 en;
    en = n;
    bl.copy_in(pos, sizeof(en), (char*)&en);
  } else
#endif
  {
    __u32 n = (__u32)(ls.size());    // FIXME: this is slow on a list.
    encode(n, bl);
    for (auto p = ls.begin(); p != ls.end(); ++p)
      encode(*p, bl, features);
  }
}
template<class T, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
  decode(std::list<T,Alloc>& ls, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  ls.clear();
  while (n--) {
    ls.emplace_back();
    decode(ls.back(), p);
  }
}

// std::list<std::shared_ptr<T>>
template<class T, class Alloc>
inline void encode(const std::list<std::shared_ptr<T>, Alloc>& ls,
		   bufferlist& bl)
{
  __u32 n = (__u32)(ls.size());  // c++11 std::list::size() is O(1)
  encode(n, bl);
  for (auto p = ls.begin(); p != ls.end(); ++p)
    encode(**p, bl);
}
template<class T, class Alloc>
inline void encode(const std::list<std::shared_ptr<T>, Alloc>& ls,
		   bufferlist& bl, uint64_t features)
{
  __u32 n = (__u32)(ls.size());  // c++11 std::list::size() is O(1)
  encode(n, bl);
  for (auto p = ls.begin(); p != ls.end(); ++p)
    encode(**p, bl, features);
}
template<class T, class Alloc>
inline void decode(std::list<std::shared_ptr<T>, Alloc>& ls,
		   bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  ls.clear();
  while (n--) {
    std::shared_ptr<T> v(std::make_shared<T>());
    decode(*v, p);
    ls.push_back(v);
  }
}

// std::set<T>
template<class T, class K, class Comp, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::set<T,Comp,Alloc>& s, K& bl)
{
  __u32 n = (__u32)(s.size());
  encode(n, bl);
  for (auto p = s.begin(); p != s.end(); ++p)
    encode(*p, bl);
}
template<class T, class Comp, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
  decode(std::set<T,Comp,Alloc>& s, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  s.clear();
  while (n--) {
    T v;
    decode(v, p);
    s.insert(v);
  }
}

template<class T, class K, class Comp, class Alloc, typename traits>
inline typename std::enable_if<!traits::supported  || std::is_base_of_v<encode_type, K> >::type
  encode_nohead(const std::set<T,Comp,Alloc>& s, K& bl)
{
  for (auto p = s.begin(); p != s.end(); ++p)
    encode(*p, bl);
}
template<class T, class Comp, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
  decode_nohead(int len, std::set<T,Comp,Alloc>& s, bufferlist::const_iterator& p)
{
  for (int i=0; i<len; i++) {
    T v;
    decode(v, p);
    s.insert(v);
  }
}

// boost::container::flat_set<T>
template<class T, class Comp, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
encode(const boost::container::flat_set<T, Comp, Alloc>& s, bufferlist& bl)
{
  __u32 n = (__u32)(s.size());
  encode(n, bl);
  for (const auto& e : s)
    encode(e, bl);
}
template<class T, class Comp, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
decode(boost::container::flat_set<T, Comp, Alloc>& s, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  s.clear();
  s.reserve(n);
  while (n--) {
    T v;
    decode(v, p);
    s.insert(v);
  }
}

template<class T, class Comp, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
encode_nohead(const boost::container::flat_set<T, Comp, Alloc>& s,
	      bufferlist& bl)
{
  for (const auto& e : s)
    encode(e, bl);
}
template<class T, class Comp, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
decode_nohead(int len, boost::container::flat_set<T, Comp, Alloc>& s,
	      bufferlist::iterator& p)
{
  s.reserve(len);
  for (int i=0; i<len; i++) {
    T v;
    decode(v, p);
    s.insert(v);
  }
}

// multiset
template<class T, class Comp, class Alloc>
inline void encode(const std::multiset<T,Comp,Alloc>& s, bufferlist& bl)
{
  __u32 n = (__u32)(s.size());
  encode(n, bl);
  for (auto p = s.begin(); p != s.end(); ++p)
    encode(*p, bl);
}
template<class T, class Comp, class Alloc>
inline void decode(std::multiset<T,Comp,Alloc>& s, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  s.clear();
  while (n--) {
    T v;
    decode(v, p);
    s.insert(v);
  }
}

template<class T, class Alloc, class K, typename traits>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::vector<T,Alloc>& v, K& bl, uint64_t features)
{
  __u32 n = (__u32)(v.size());
  encode(n, bl);
  for (auto p = v.begin(); p != v.end(); ++p)
    encode(*p, bl, features);
}
template<class T, class Alloc, class K, typename traits>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::vector<T,Alloc>& v, K& bl)
{
  __u32 n = (__u32)(v.size());
  encode(n, bl);
  for (auto p = v.begin(); p != v.end(); ++p)
    encode(*p, bl);
}
template<class T, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
  decode(std::vector<T,Alloc>& v, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  v.resize(n);
  for (__u32 i=0; i<n; i++) 
    decode(v[i], p);
}

template<class T, class Alloc, class K, typename traits>
inline std::enable_if_t<!traits::supported || std::is_base_of_v<encode_type, K> >
  encode_nohead(const std::vector<T,Alloc>& v, K& bl)
{
  for (auto p = v.begin(); p != v.end(); ++p)
    encode(*p, bl);
}
template<class T, class Alloc, typename traits>
inline std::enable_if_t<!traits::supported>
  decode_nohead(int len, std::vector<T,Alloc>& v, bufferlist::const_iterator& p)
{
  v.resize(len);
  for (__u32 i=0; i<v.size(); i++) 
    decode(v[i], p);
}

// vector (shared_ptr)
template<class T,class Alloc, class K>
inline void encode(const std::vector<std::shared_ptr<T>,Alloc>& v,
		   K& bl,
		   uint64_t features)
{
  __u32 n = (__u32)(v.size());
  encode(n, bl);
  for (auto p = v.begin(); p != v.end(); ++p)
    if (*p)
      encode(**p, bl, features);
    else
      encode(T(), bl, features);
}
template<class T, class Alloc, class K>
inline void encode(const std::vector<std::shared_ptr<T>,Alloc>& v,
		   K& bl)
{
  __u32 n = (__u32)(v.size());
  encode(n, bl);
  for (auto p = v.begin(); p != v.end(); ++p)
    if (*p)
      encode(**p, bl);
    else
      encode(T(), bl);
}
template<class T, class Alloc>
inline void decode(std::vector<std::shared_ptr<T>,Alloc>& v,
		   bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  v.resize(n);
  for (__u32 i=0; i<n; i++) {
    v[i] = std::make_shared<T>();
    decode(*v[i], p);
  }
}

// map
template<class T, class U, class K, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
inline std::enable_if_t<!t_traits::supported ||	!u_traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::map<T,U,Comp,Alloc>& m, K& bl)
{
  __u32 n = (__u32)(m.size());
  encode(n, bl);
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl);
    encode(p->second, bl);
  }
}
template<class T, class U, class K, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported || std::is_base_of_v<encode_type, K> >
  encode(const std::map<T,U,Comp,Alloc>& m, K& bl, uint64_t features)
{
  __u32 n = (__u32)(m.size());
  encode(n, bl);
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl, features);
    encode(p->second, bl, features);
  }
}
template<class T, class U, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
  decode(std::map<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  m.clear();
  while (n--) {
    T k;
    decode(k, p);
    decode(m[k], p);
  }
}
template<class T, class U, class Comp, class Alloc>
inline void decode_noclear(std::map<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  while (n--) {
    T k;
    decode(k, p);
    decode(m[k], p);
  }
}
template<class T, class U, class Comp, class Alloc, class K,
	 typename t_traits, typename u_traits>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported || std::is_base_of_v<encode_type, K> >
  encode_nohead(const std::map<T,U,Comp,Alloc>& m, K& bl)
{
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl);
    encode(p->second, bl);
  }
}
template<class T, class U, class Comp, class Alloc, class K,
	 typename t_traits, typename u_traits>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported || std::is_base_of_v<encode_type, K> >
  encode_nohead(const std::map<T,U,Comp,Alloc>& m, K& bl, uint64_t features)
{
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl, features);
    encode(p->second, bl, features);
  }
}
template<class T, class U, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
  decode_nohead(int n, std::map<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p)
{
  m.clear();
  while (n--) {
    T k;
    decode(k, p);
    decode(m[k], p);
  }
}

// boost::container::flat-map
template<class T, class U, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
  inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
  encode(const boost::container::flat_map<T,U,Comp,Alloc>& m, bufferlist& bl)
{
  __u32 n = (__u32)(m.size());
  encode(n, bl);
  for (typename boost::container::flat_map<T,U,Comp>::const_iterator p
	 = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl);
    encode(p->second, bl);
  }
}
template<class T, class U, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
  inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
  encode(const boost::container::flat_map<T,U,Comp,Alloc>& m, bufferlist& bl,
	 uint64_t features)
{
  __u32 n = (__u32)(m.size());
  encode(n, bl);
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl, features);
    encode(p->second, bl, features);
  }
}
template<class T, class U, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
  inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
  decode(boost::container::flat_map<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  m.clear();
  m.reserve(n);
  while (n--) {
    T k;
    decode(k, p);
    decode(m[k], p);
  }
}
template<class T, class U, class Comp, class Alloc>
inline void decode_noclear(boost::container::flat_map<T,U,Comp,Alloc>& m,
			   bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  m.reserve(m.size() + n);
  while (n--) {
    T k;
    decode(k, p);
    decode(m[k], p);
  }
}
template<class T, class U, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
  inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
  encode_nohead(const boost::container::flat_map<T,U,Comp,Alloc>& m,
		bufferlist& bl)
{
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl);
    encode(p->second, bl);
  }
}
template<class T, class U, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
  inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
  encode_nohead(const boost::container::flat_map<T,U,Comp,Alloc>& m,
		bufferlist& bl, uint64_t features)
{
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl, features);
    encode(p->second, bl, features);
  }
}
template<class T, class U, class Comp, class Alloc,
	 typename t_traits, typename u_traits>
inline std::enable_if_t<!t_traits::supported || !u_traits::supported>
  decode_nohead(int n, boost::container::flat_map<T,U,Comp,Alloc>& m,
		bufferlist::const_iterator& p)
{
  m.clear();
  while (n--) {
    T k;
    decode(k, p);
    decode(m[k], p);
  }
}

// multimap
template<class T, class K, class U, class Comp, class Alloc>
inline void encode(const std::multimap<T,U,Comp,Alloc>& m, K& bl)
{
  __u32 n = (__u32)(m.size());
  encode(n, bl);
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl);
    encode(p->second, bl);
  }
}
template<class T, class U, class Comp, class Alloc>
inline void decode(std::multimap<T,U,Comp,Alloc>& m, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  m.clear();
  while (n--) {
    typename std::pair<T,U> tu = std::pair<T,U>();
    decode(tu.first, p);
    typename std::multimap<T,U,Comp,Alloc>::iterator it = m.insert(tu);
    decode(it->second, p);
  }
}

// ceph::unordered_map
template<class T, class U, class Hash, class Pred, class Alloc, class K>
inline void encode(const unordered_map<T,U,Hash,Pred,Alloc>& m, K& bl,
		   uint64_t features)
{
  __u32 n = (__u32)(m.size());
  encode(n, bl);
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl, features);
    encode(p->second, bl, features);
  }
}
template<class T, class U, class Hash, class Pred, class Alloc, class K>
inline void encode(const unordered_map<T,U,Hash,Pred,Alloc>& m, K& bl)
{
  __u32 n = (__u32)(m.size());
  encode(n, bl);
  for (auto p = m.begin(); p != m.end(); ++p) {
    encode(p->first, bl);
    encode(p->second, bl);
  }
}
template<class T, class U, class Hash, class Pred, class Alloc>
inline void decode(unordered_map<T,U,Hash,Pred,Alloc>& m, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  m.clear();
  while (n--) {
    T k;
    decode(k, p);
    decode(m[k], p);
  }
}

// ceph::unordered_set
template<class T, class K, class Hash, class Pred, class Alloc>
inline void encode(const ceph::unordered_set<T,Hash,Pred,Alloc>& m, K& bl)
{
  __u32 n = (__u32)(m.size());
  encode(n, bl);
  for (auto p = m.begin(); p != m.end(); ++p)
    encode(*p, bl);
}
template<class T, class Hash, class Pred, class Alloc>
inline void decode(ceph::unordered_set<T,Hash,Pred,Alloc>& m, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  m.clear();
  while (n--) {
    T k;
    decode(k, p);
    m.insert(k);
  }
}

// deque
template<class T, class Alloc>
inline void encode(const std::deque<T,Alloc>& ls, bufferlist& bl, uint64_t features)
{
  __u32 n = ls.size();
  encode(n, bl);
  for (auto p = ls.begin(); p != ls.end(); ++p)
    encode(*p, bl, features);
}
template<class T, class Alloc>
inline void encode(const std::deque<T,Alloc>& ls, bufferlist& bl)
{
  __u32 n = ls.size();
  encode(n, bl);
  for (auto p = ls.begin(); p != ls.end(); ++p)
    encode(*p, bl);
}
template<class T, class Alloc>
inline void decode(std::deque<T,Alloc>& ls, bufferlist::const_iterator& p)
{
  __u32 n;
  decode(n, p);
  ls.clear();
  while (n--) {
    ls.emplace_back();
    decode(ls.back(), p);
  }
}

// std::array<T, N>
template<class T, size_t N, typename traits>
inline std::enable_if_t<!traits::supported>
encode(const std::array<T, N>& v, bufferlist& bl, uint64_t features)
{
  for (const auto& e : v)
    encode(e, bl, features);
}
template<class T, size_t N, typename traits>
inline std::enable_if_t<!traits::supported>
encode(const std::array<T, N>& v, bufferlist& bl)
{
  for (const auto& e : v)
    encode(e, bl);
}
template<class T, size_t N, typename traits>
inline std::enable_if_t<!traits::supported>
decode(std::array<T, N>& v, bufferlist::const_iterator& p)
{
  for (auto& e : v)
    decode(e, p);
}
}

/*
 * guards
 */

/**
 * start encoding block
 *
 * @param v current (code) version of the encoding
 * @param compat oldest code version that can decode it
 * @param bl bufferlist to encode to
 *
 */
#define ENCODE_START(v, compat, bl)			     \
  __u8 struct_v = v;                                         \
  __u8 struct_compat = compat;		                     \
  ceph_le32 struct_len;				             \
  auto /*::buffer::list::contiguous_filler*/ filler =		     \
    (bl).append_hole(2 * sizeof(__u8) + sizeof(ceph_le32));  \
  const auto starting_bl_len = (bl).length();		     \
  using ::ceph::encode;					     \
  do {

/**
 * finish encoding block
 *
 * @param bl bufferlist we were encoding to
 * @param new_struct_compat struct-compat value to use
 */
#define ENCODE_FINISH_NEW_COMPAT(bl, new_struct_compat)      \
  } while (false);                                           \
  if (new_struct_compat) {                                   \
    struct_compat = new_struct_compat;                       \
  }                                                          \
  struct_len = (bl).length() - starting_bl_len;              \
  filler.copy_in(1u, (char *)&struct_v);                     \
  filler.copy_in(1u, (char *)&struct_compat);                \
  filler.copy_in(4u, (char *)&struct_len);

#define ENCODE_FINISH(bl) ENCODE_FINISH_NEW_COMPAT(bl, 0)

#define DECODE_ERR_OLDVERSION(func, v, compatv)					\
  (std::string(func) + " no longer understand old encoding version " #v " < " + std::to_string(compatv))

#define DECODE_ERR_PAST(func) \
  (std::string(func) + " decode past end of struct encoding")

/**
 * check for very old encoding
 *
 * If the encoded data is older than oldestv, raise an exception.
 *
 * @param oldestv oldest version of the code we can successfully decode.
 */
#define DECODE_OLDEST(oldestv)						\
  if (struct_v < oldestv)						\
    throw ::ceph::buffer::malformed_input(DECODE_ERR_OLDVERSION(__PRETTY_FUNCTION__, v, oldestv)); 

/**
 * start a decoding block
 *
 * @param v current version of the encoding that the code supports/encodes
 * @param bl bufferlist::iterator for the encoded data
 */
#define DECODE_START(v, bl)						\
  __u8 struct_v, struct_compat;						\
  using ::ceph::decode;							\
  decode(struct_v, bl);						\
  decode(struct_compat, bl);						\
  if (v < struct_compat)						\
    throw buffer::malformed_input(DECODE_ERR_OLDVERSION(__PRETTY_FUNCTION__, v, struct_compat)); \
  __u32 struct_len;							\
  decode(struct_len, bl);						\
  if (struct_len > bl.get_remaining())					\
    throw ::ceph::buffer::malformed_input(DECODE_ERR_PAST(__PRETTY_FUNCTION__)); \
  unsigned struct_end = bl.get_off() + struct_len;			\
  do {

#define __DECODE_START_LEGACY_COMPAT_LEN(v, compatv, lenv, skip_v, bl)	\
  using ::ceph::decode;							\
  __u8 struct_v;							\
  decode(struct_v, bl);						\
  if (struct_v >= compatv) {						\
    __u8 struct_compat;							\
    decode(struct_compat, bl);					\
    if (v < struct_compat)						\
      throw buffer::malformed_input(DECODE_ERR_OLDVERSION(__PRETTY_FUNCTION__, v, struct_compat)); \
  } else if (skip_v) {							\
    if (bl.get_remaining() < skip_v)					\
      throw buffer::malformed_input(DECODE_ERR_PAST(__PRETTY_FUNCTION__)); \
    bl.advance(skip_v);							\
  }									\
  unsigned struct_end = 0;						\
  if (struct_v >= lenv) {						\
    __u32 struct_len;							\
    decode(struct_len, bl);						\
    if (struct_len > bl.get_remaining())				\
      throw buffer::malformed_input(DECODE_ERR_PAST(__PRETTY_FUNCTION__)); \
    struct_end = bl.get_off() + struct_len;				\
  }									\
  do {

/**
 * start a decoding block with legacy support for older encoding schemes
 *
 * The old encoding schemes has a __u8 struct_v only, or lacked either
 * the compat version or length.  Skip those fields conditionally.
 *
 * Most of the time, v, compatv, and lenv will all match the version
 * where the structure was switched over to the new macros.
 *
 * @param v current version of the encoding that the code supports/encodes
 * @param compatv oldest version that includes a __u8 compat version field
 * @param lenv oldest version that includes a __u32 length wrapper
 * @param bl bufferlist::iterator containing the encoded data
 */
#define DECODE_START_LEGACY_COMPAT_LEN(v, compatv, lenv, bl)		\
  using ::ceph::decode;							\
  __u8 struct_v;							\
  decode(struct_v, bl);							\
  if (struct_v >= compatv) {						\
    __u8 struct_compat;							\
    decode(struct_compat, bl);						\
    if (v < struct_compat)						\
      throw buffer::malformed_input(DECODE_ERR_OLDVERSION(		\
	__PRETTY_FUNCTION__, v, struct_compat));			\
  }									\
  unsigned struct_end = 0;						\
  if (struct_v >= lenv) {						\
    __u32 struct_len;							\
    decode(struct_len, bl);						\
    if (struct_len > bl.get_remaining())				\
      throw buffer::malformed_input(DECODE_ERR_PAST(__PRETTY_FUNCTION__)); \
    struct_end = bl.get_off() + struct_len;				\
  }									\
  do {

/**
 * start a decoding block with legacy support for older encoding schemes
 *
 * This version of the macro assumes the legacy encoding had a 32 bit
 * version
 *
 * The old encoding schemes has a __u8 struct_v only, or lacked either
 * the compat version or length.  Skip those fields conditionally.
 *
 * Most of the time, v, compatv, and lenv will all match the version
 * where the structure was switched over to the new macros.
 *
 * @param v current version of the encoding that the code supports/encodes
 * @param compatv oldest version that includes a __u8 compat version field
 * @param lenv oldest version that includes a __u32 length wrapper
 * @param bl bufferlist::iterator containing the encoded data
 */
#define DECODE_START_LEGACY_COMPAT_LEN_32(v, compatv, lenv, bl)		\
  __DECODE_START_LEGACY_COMPAT_LEN(v, compatv, lenv, 3u, bl)

#define DECODE_START_LEGACY_COMPAT_LEN_16(v, compatv, lenv, bl)		\
  __DECODE_START_LEGACY_COMPAT_LEN(v, compatv, lenv, 1u, bl)

/**
 * finish decode block
 *
 * @param bl bufferlist::iterator we were decoding from
 */
#define DECODE_FINISH(bl)						\
  } while (false);							\
  if (struct_end) {							\
    if (bl.get_off() > struct_end)					\
      throw buffer::malformed_input(DECODE_ERR_PAST(__PRETTY_FUNCTION__)); \
    if (bl.get_off() < struct_end)					\
      bl.advance(struct_end - bl.get_off());				\
  }

namespace ceph {

/*
 * Encoders/decoders to read from current offset in a file handle and
 * encode/decode the data according to argument types.
 */
inline ssize_t decode_file(int fd, std::string &str)
{
  bufferlist bl;
  __u32 len = 0;
  bl.read_fd(fd, sizeof(len));
  decode(len, bl);                                                                                                  
  bl.read_fd(fd, len);
  decode(str, bl);                                                                                                  
  return bl.length();
}

inline ssize_t decode_file(int fd, bufferptr &bp)
{
  bufferlist bl;
  __u32 len = 0;
  bl.read_fd(fd, sizeof(len));
  decode(len, bl);
  bl.read_fd(fd, len);
  auto bli = std::cbegin(bl);

  decode(bp, bli);
  return bl.length();
}
}

#endif
