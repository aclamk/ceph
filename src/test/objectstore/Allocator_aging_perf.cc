// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Bitmap allocator performation simulation.
 * Author: Adam Kupczyk, akupczyk@redhat.com
 */
#include <iostream>
#include <boost/scoped_ptr.hpp>
#include "gtest/gtest.h"
#include <boost/random/triangle_distribution.hpp>

#include "common/ceph_mutex.h"
#include "common/Cond.h"
#include "common/errno.h"
#include "include/stringify.h"
#include "include/Context.h"
#include "os/bluestore/Allocator.h"
#include "os/bluestore/AvlAllocator.h"
#include <boost/random/uniform_int.hpp>

typedef boost::mt11213b gen_type;

#include "common/debug.h"
#define dout_context g_ceph_context
#define dout_subsys ceph_subsys_


bool verbose = getenv("VERBOSE") != nullptr;

class AllocTest : public ::testing::TestWithParam<std::tuple<std::string, uint32_t, uint32_t> > {
protected:
  gen_type rng;
public:
  boost::scoped_ptr<Allocator> alloc;
  AllocTest(): alloc(nullptr) {}
  void init_alloc(const std::string& alloc_name, int64_t size, uint64_t min_alloc_size);
  void init_close();

  uint64_t capacity;
  uint32_t alloc_unit;
};



const uint64_t _1m = 1024 * 1024;
const uint64_t _1G = 1024 * 1024 * 1024;

struct index_tab
{
  uint32_t l_count = 0;
  uint64_t l_weight = 0;
};

struct element
{
  std::vector<bluestore_pextent_t> allocations;
};

class Index
{
  index_tab total;
  uint32_t index_depth = 1;
  std::vector<index_tab> index;
  std::vector<element> elements;
  uint32_t element_count = 0;
  std::vector<uint32_t> freed_locations;

public:
  Index()
  {
    index.resize(2);
    elements.resize(2);
  }

  uint64_t getWeight() {
    return total.l_weight;
  }
  uint32_t getCount() {
    return total.l_count;
  }
  uint32_t getDepth() {
    return index_depth;
  }

  void addElement(uint32_t weight, const element& x)
  {
    uint32_t id = 0;
    uint32_t pos = 0;
    if (total.l_count == (1u << index_depth) ) {
      addLevel();
      //needs more space
    }

    total.l_count++;
    total.l_weight += weight;
    for (uint32_t i = 0; i < index_depth; i++) {
      uint32_t half = 1 << (index_depth - 1 - i);
      if (index[pos].l_count < half) {
	index[pos].l_count++;
	index[pos].l_weight += weight;
	pos = pos + 1;//(1 << i);
	id = id * 2;
      } else {
	pos = pos + half;//(1 << i) + 1;
	id = id * 2 + 1;
      }
    }
    elements[id] = x;
  }

  void searchElement(uint64_t search_weight, uint32_t* o_id, uint32_t* o_weight, element** o_x)
  {
    uint32_t id = 0;
    uint32_t pos = 0;
    uint64_t weight = total.l_weight;
    std::vector<uint32_t> path;
    ceph_assert(search_weight < total.l_weight);
    path.reserve(index_depth);
    for (uint32_t i = 0; i < index_depth; i++) {
      path.push_back(pos);
      if (search_weight < index[pos].l_weight) {
	weight = index[pos].l_weight;
	pos = pos + 1;
	id = id * 2;
      } else {
	weight = weight - index[pos].l_weight;
	search_weight -= index[pos].l_weight;
	pos = pos + (1 << (index_depth -1 - i));
	id = id * 2 + 1;
      }
    }
    *o_x = &elements[id];
    *o_id = id;
    *o_weight = weight;
  }


  void addLevel()
  {
    index_depth++;
    index.resize(1 << index_depth);
    index.insert(index.begin(), total);
    elements.resize(1 << index_depth);
  }

  element removeElement(uint64_t search_weight)
  {
    uint32_t id = 0;
    uint32_t pos = 0;
    uint64_t weight = total.l_weight;
    std::vector<uint32_t> path;
    path.reserve(index_depth);
    for (uint32_t i = 0; i < index_depth; i++) {
      //weight = search_weight;
      if (search_weight < index[pos].l_weight) {
	path.push_back(pos);
	weight = index[pos].l_weight;
	pos = pos + 1;
	id = id * 2;
      } else {
	weight = weight - index[pos].l_weight;
	search_weight -= index[pos].l_weight;
	pos = pos + (1 << (index_depth -1 - i));
	id = id * 2 + 1;
      }
    }
    element result = elements[pos];
    elements[id] = element{};
    for (auto i : path) {
      index[i].l_count--;
      index[i].l_weight -= weight;
    }
    total.l_count--;
    total.l_weight -= weight;
    return result;
  }

  void dump()
  {
    std::cout << "cnt = " << total.l_count << std::endl;
    std::cout << "weight = " << total.l_weight << std::endl;
    for (size_t i = 0; i < index.size(); i++) {
      std::cout << "i=" << i << " c=" << index[i].l_count << " w=" << index[i].l_weight << std::endl;
    }
    for (size_t i = 0; i < elements.size(); i++) {
      std::cout << "e=" << i << " " << elements[i].allocations << std::endl;
    }
  }

};

TEST(Set, dupa)
{
  Index x;

  {
  element e;
  e.allocations.emplace_back(0,100);
  x.addElement(100,e);
  }
  ASSERT_EQ(1, x.getCount());
  ASSERT_EQ(100, x.getWeight());
  ASSERT_EQ(1, x.getDepth());
  x.dump();
  {
  element e;
  e.allocations.emplace_back(0,5000);
  x.addElement(1000000,e);
  }

  ASSERT_EQ(2, x.getCount());
  ASSERT_EQ(1000100, x.getWeight());
  ASSERT_EQ(1, x.getDepth());
  x.dump();

  uint32_t o_id;
  uint32_t o_weight;
  element* o_x;
  x.searchElement(1000, &o_id, &o_weight, &o_x);
  ASSERT_EQ(1, o_id);
  ASSERT_EQ(1000000, o_weight);
  ASSERT_EQ(o_x->allocations[0].length, 5000);
}

TEST(Set, zupa)
{
  Index x;
  for(size_t i = 0; i < 1000 ; i++) {
    element e;
    e.allocations.emplace_back(0,i);
    x.addElement(1,e);
    ASSERT_EQ(i + 1, x.getCount());
    ASSERT_EQ(i + 1, x.getWeight());
  }
  for(size_t i = 0; i < 1000 ; i++) {
    uint32_t o_id;
    uint32_t o_weight;
    element* o_x;
    x.searchElement(i, &o_id, &o_weight, &o_x);
    ASSERT_EQ(i, o_id);
    ASSERT_EQ(1, o_weight);
    ASSERT_EQ(i, o_x->allocations[0].length);
  }

  for(size_t j = 0; j < 1000 ; j++) {
    size_t i = 1000 - j;
    ASSERT_EQ(i, x.getWeight());
    ASSERT_EQ(i, x.getCount());
    element e = x.removeElement((j * 3 * 7 * 11) % x.getWeight());
  }
}

TEST(Set, kant)
{
  Index x;
  for(size_t i = 0; i < 1000 ; i++) {
    element e;
    e.allocations.emplace_back(0,i);
    x.addElement(i + 1, e);
    ASSERT_EQ(i + 1, x.getCount());
    ASSERT_EQ((i + 2) * (i + 1) / 2, x.getWeight());
  }
  //x.dump();
  std::vector<uint32_t> p;
  p.resize(1000);
  for(size_t i = 0; i < 1000 * 1001 / 2 ; i++) {
    uint32_t o_id;
    uint32_t o_weight;
    element* o_x;
    x.searchElement(i, &o_id, &o_weight, &o_x);
    //ASSERT_EQ(i, o_id);
    //ASSERT_EQ(1, o_weight);
    //ASSERT_EQ(i, o_x->allocations[0].length);
    p[o_weight - 1]++;
    ASSERT_EQ(o_weight - 1, o_x->allocations[0].length);
  }
  for (size_t i = 0; i < 1000; i++) {
    ASSERT_EQ(i + 1, p[i]);
  }
}


void generate_image(size_t capacity, size_t alloc_unit, std::map<size_t, size_t>& all, const char* name = nullptr)
{
  size_t w = 512;
  size_t h = 512;
  size_t pixel_size = capacity/(w*h);

  uint8_t* data = new uint8_t[w*h*3];
  uint32_t fragment_gradation = pixel_size / alloc_unit / 32;
  if (fragment_gradation < 4) fragment_gradation = 4;
  all[capacity] = 0;
  auto it = all.begin();
  for (size_t i=0; i<w*h; i++) {
    size_t begin = pixel_size * i;
    size_t limit = pixel_size * (i+1);
    size_t free_count = 0;
    size_t free_size = 0;
    while (it->first < limit) {
      free_count ++;
      if (it->first < begin) {
	if (it->first + it->second > limit) {
	  free_size += limit - begin;
	  break;
	} else {
	  free_size += it->first + it->second - begin;
	  it++;
	}
      } else {
	if (it->first + it->second > limit) {
	  free_size += limit - it->first;
	  break;
	} else {
	  free_size += it->first + it->second - it->first;
	  it++;
	}
      }
    }
    uint8_t b = ((1 - free_size*1.0 / pixel_size) * 128);
    uint8_t r = 0;
    uint8_t g = 0;
    if (free_count > 1) {
      double t;
      t = double(free_count - 1) / (fragment_gradation) + 0.25;
      if (t>1) t = 1;
      r = g = b = 255 * (t);
    }
    data[i*3+0] = r;
    data[i*3+1] = g;
    data[i*3+2] = b;
  }
  static int fid=0;
  int fd = open(
		(name?name:std::string("bitmap-")+to_string(fid)).c_str(), O_WRONLY|O_CREAT, 0666);
  ceph_assert(fd > 0);
  write(fd, data, w*h*3);
  close(fd);
  fid++;
}

void generate_image(size_t capacity, size_t alloc_unit, Allocator* alloc, const char* name = nullptr)
{
  std::map<size_t, size_t> all;
  auto list_free = [&](size_t off, size_t len) {
    all[off] = len;
    //std::cout << off << " " << len << std::endl;
  };
  alloc->dump(list_free);
  generate_image(capacity, alloc_unit, all, name);
}


void AllocTest::init_alloc(const std::string& allocator_name, int64_t size, uint64_t min_alloc_size) {
  this->capacity = size;
  this->alloc_unit = min_alloc_size;
  rng.seed(0);
  alloc.reset(Allocator::create(g_ceph_context, allocator_name, size,
				min_alloc_size));
}

void AllocTest::init_close() {
    alloc.reset(0);
}


TEST_P(AllocTest, visualization)
{
  auto const & param = GetParam();
  std::string allocator_name = std::get<0>(param);
  uint32_t cap_G = std::get<1>(param);
  uint64_t capacity = cap_G * _1G;
  uint32_t alloc_unit_k = std::get<2>(param);
  uint64_t alloc_unit = alloc_unit_k * 1024;

  ceph_assert((alloc_unit & (alloc_unit - 1)) == 0);

  const char* e = getenv("IMAGES");
  bool images = (e && strcmp(e,"1")==0);

  uint64_t max_fill = capacity * 0.95;
  double increase = 1.0 / 2000;
  std::cout << "allocator=" << allocator_name <<
    " capacity=" << cap_G << "G alloc_unit=" << alloc_unit_k << "k" << std::endl;

  init_alloc(allocator_name, capacity, alloc_unit);
  alloc->init_add_free(0, capacity);

  gen_type random;

  auto freq_use = [&]() ->uint32_t {
    double x = (double)random() / std::numeric_limits<uint32_t>::max();
    double v = 1*(1 - x*x*x*x) + 10*(x*x*x*x);
    return v*100000;
  };

  auto object_size = [&]() -> uint32_t {
    double x = (double)random() / std::numeric_limits<uint32_t>::max();
    if (x > 0.95)
      return 8 * 1024 * 1024;
    x = x / 0.95;
    double v = (9*(x*x*x*x*x) + x*x) / 10;
    uint32_t size = v * (8 * 1024 * 1024 - 1) + 1;
    return size;
  };

  Index objects;

  uint32_t objs = 0;
  uint32_t frags = 0;
  double sumtime = 0;
  double totaltime = 0;
  std::string all_images;

  for (size_t kk = 0; kk<2000; kk++) {

    for (size_t substep = 0; substep < 10; substep++) {
      uint64_t target_fill = (kk * 10 + substep + 1) * capacity * increase / 10;
      if (target_fill > max_fill)
	target_fill = max_fill;
      while ((capacity - alloc->get_free()) < target_fill) {
	PExtentVector tmp;
	auto size = object_size();
	auto freq = freq_use();
	utime_t start = ceph_clock_now();
	alloc->allocate((size + alloc_unit - 1) / alloc_unit * alloc_unit, alloc_unit, 0, 0, &tmp);
	sumtime += ceph_clock_now() - start;
	element e;
	for (auto& t: tmp) {
	  e.allocations.emplace_back(t.offset, t.length);
	}
	objects.addElement(freq, e);
	objs++;
	frags+=e.allocations.size();
      }

      for (size_t i = 0; i<1000; i++) {
	uint64_t w = (double)random() / std::numeric_limits<uint32_t>::max() * objects.getWeight();
	uint32_t o_id;
	uint32_t o_weight;
	element* o_x;
	objects.searchElement(w, &o_id, &o_weight, &o_x);
	uint32_t size = 0;
	interval_set<uint64_t> release_set;

	frags-=o_x->allocations.size();
	for (auto& t: o_x->allocations) {
	  release_set.insert(t.offset, t.length);
	  size = size + t.length;
	}

	o_x->allocations.clear();

	PExtentVector tmp;
	utime_t start = ceph_clock_now();
	alloc->allocate((size + alloc_unit - 1) / alloc_unit * alloc_unit, alloc_unit, 0, 0, &tmp);
	sumtime += ceph_clock_now() - start;
	for (auto& t: tmp) {
	  o_x->allocations.emplace_back(t.offset, t.length);
	}
	frags+=o_x->allocations.size();
	start = ceph_clock_now();
	alloc->release(release_set);
	sumtime += ceph_clock_now() - start;
      }
    }
    if ((kk%10) == 9)
    {
      // for i in $(ls -tr bitmap-*[0-9]); do magick convert -size 512x512 -depth 8 rgb:$i B$i.png; magick convert B$i.png -draw 'rectangle 0,494,300,474' -fill white -annotate +10+490 "$(cat $i.caption)" C$i.png; done
      char buf[200];
      sprintf(buf, "objs=%u frag=%2.2lf free=%2.2lf time=%1.1lf mem=%luk",
	       objs, (double)frags/objs,
	       (double)alloc->get_free() / capacity, sumtime*1000,
	      mempool::bluestore_alloc::allocated_bytes() / 1024u);
      std::cout << kk/10 << ": " << buf << std::endl;

      if (images) {
	generate_image(capacity, alloc_unit, alloc.get(), "rgb-tmp");
	std::string image_file = "rgb-" + to_string(kk/10) + ".png";
	std::string command = "magick convert -size 512x512 -depth 8 rgb:rgb-tmp tmp.png; "
	  "magick convert tmp.png -draw 'rectangle 0,494,300,474' -fill white -annotate +10+490 "
	  "\"" + std::string(buf) + "\" " + image_file;

	system(command.c_str());
	all_images += image_file + " ";
      }

      //AvlAllocator* v = dynamic_cast<AvlAllocator*>(alloc.get());
      //if (v) {
      //std::cout << "tree_size=" << v->range_size_tree.size() << " range_count_cap=" << v->range_count_cap << std::endl;
      //}
      totaltime += sumtime;
      sumtime = 0;
    }
    if (totaltime + sumtime > 3600) {
      std::cout << "timeout. bailing out" << std::endl;
      break;
    }
  }
  if (images) {
    system(("magick convert -delay 20 " + all_images +
	    allocator_name + "-" + to_string(cap_G) + "G-" + to_string(alloc_unit_k) + "k.gif").c_str());
    system(("rm "+all_images).c_str());
  }
}

INSTANTIATE_TEST_CASE_P(
  Allocator,
  AllocTest,
  ::testing::Combine(
		     ::testing::Values("stupid", "bitmap", "avl", "hybrid"),
		     ::testing::Values(512, 1024, 2048, 4096),
		     ::testing::Values(4, 64)
		     ));
