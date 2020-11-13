#ifndef _CEPH_INCLUDE_TLS_ALLOC_H
#define _CEPH_INCLUDE_TLS_ALLOC_H
#include <utility>
#include <cstddef>
#include <vector>
/* 
   this has context and has actual meaningful instances 
 */

/*
  Objects need to have domains. Different groups of objects will go into different domains.
  Currently only onode_cache domain is utilized.
 */
class Onode;
class OnodeRef;
class onode_alloc {
  static constexpr size_t max_align = alignof(max_align_t);   // exposition only
  static thread_local onode_alloc* onode_alloc_tls;
public:
  static void set_alloc(onode_alloc* a) {
    onode_alloc_tls = a;
  }
  static void clear_alloc() {
    onode_alloc_tls = nullptr;
  }
  static onode_alloc* get_alloc() {
    return onode_alloc_tls;
  }
  void* allocate(size_t bytes, size_t alignment = max_align);
  void deallocate(void* p, size_t bytes, size_t alignment = max_align);
  void list_onodes();

  size_t get_free();
  size_t get_allocated();

  /* signals Onodes that use this as primary container */
  void add_onode(OnodeRef* o);
  /* copy Onode from some other container here */
  Onode* copy(Onode*);
private:
  void* storage;
  size_t size;
  size_t alloc_pos; /// next allocation position
  size_t free_cnt;  /// count of bytes in range <0..alloc_pos) that is already freed
  
};

/*
  !!!!!
  Need some interface to purge entire regions.
  
*/

/* manager for onode_alloc instances */
class onode_alloc_mgr {
public:
  onode_alloc_mgr() {};
  ~onode_alloc_mgr() {};
  onode_alloc* alloc_for_onode();
private:
  std::vector<onode_alloc*> alloc_groups;
};

/* 
 allocator 
*/

template<typename G, typename T>
class tls_allocator
{
public:
  typedef tls_allocator<G, T> allocator_type;
  typedef T value_type;
  typedef value_type *pointer;
  typedef const value_type * const_pointer;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;

  template<typename U> struct rebind {
    typedef tls_allocator<G, U> other;
  };

  tls_allocator() {
  }
  
  template<typename U>
  tls_allocator(const tls_allocator<G, U>&) {
  }

  T* allocate(size_t n, void *p = nullptr) {
    auto alloc = G::get_alloc();
    void* v = alloc->allocate(n * sizeof(T), alignof(T));
    T* r = reinterpret_cast<T*>(v);
    return r;
  }

  void deallocate(T* p, size_t n) {
    auto alloc = G::get_alloc();
    alloc->deallocate(p, n * sizeof(T), alignof(T));
  }
  
  void destroy(T* p) {
    p->~T();
  }

  template<class U>
  void destroy(U *p) {
    p->~U();
  }

  void construct(T* p, const T& val) {
    ::new ((void *)p) T(val);
  }

  template<class U, class... Args> void construct(U* p,Args&&... args) {
    ::new((void *)p) U(std::forward<Args>(args)...);
  }

  bool operator==(const tls_allocator&) const { return true; }
  bool operator!=(const tls_allocator&) const { return false; }
};

//interchangeable with MEMPOOL_CLASS_HELPERS
#define TLS_ALLOC_CLASS_HELPERS()					\
  void *operator new(size_t size);					\
  void *operator new[](size_t size) noexcept {				\
    ceph_abort_msg("no array new");					\
    return nullptr; }							\
  void  operator delete(void *);					\
  void  operator delete[](void *) { ceph_abort_msg("no array delete"); }


// Use this in some particular .cc file to match each class with a
#define TLS_ALLOC_DEFINE_OBJECT_FACTORY(obj,alloc_name)	                \
  void *obj::operator new(size_t size) {				\
    auto alloc = alloc_name::get_alloc();                               \
    void* v = alloc->allocate(sizeof(obj), alignof(obj));               \
    return reinterpret_cast<obj*>(v);                                   \
  }									\
  void obj::operator delete(void *p)  {					\
    auto alloc = alloc_name::get_alloc();                               \
    alloc->deallocate(p, sizeof(obj), alignof(obj));                    \
  }



#endif
