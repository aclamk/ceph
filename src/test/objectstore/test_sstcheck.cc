// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <random>
#include <thread>
#include <stack>
#include "global/global_init.h"
#include "common/ceph_argparse.h"
#include "include/stringify.h"
#include "include/scope_guard.h"
#include "common/errno.h"
#include <gtest/gtest.h>

#include "os/bluestore/BlueFS.h"

#include "os/bluestore/SstCheck.h"
#include "common/pretty_binary.h"

#include "common/debug.h"
//#include "common/errno.h"
//#include "include/ceph_assert.h"
//#include "common/ceph_context.h"
//#include "global/global_context.h"

#define dout_context g_ceph_context
#define dout_subsys ceph_subsys_bluefs
#undef dout_prefix
#define dout_prefix *_dout << "sst "


using namespace std;

std::unique_ptr<char[]> gen_buffer(uint64_t size)
{
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size);
    std::independent_bits_engine<std::default_random_engine, CHAR_BIT, unsigned char> e;
    std::generate(buffer.get(), buffer.get()+size, std::ref(e));
    return buffer;
}

struct sst_data {
  std::string desc;
  const char* bytes;
  size_t size;
};
extern std::vector<sst_data> test_surveyor_data;



TEST(TestSstCheck, mark) {

  constexpr size_t tests = 1000;
  constexpr size_t marks_per_test = 100;
  constexpr size_t tab_size = 50;
  constexpr size_t levels = 5;
  int tab[tab_size];

  for (size_t t = 0; t < tests; t++) {
    SstCheck S(g_ceph_context);
    //std::cout << "-----" << std::endl;
    for (size_t i = 0; i < tab_size; i++) {
      tab[i] = -1;
    }

    size_t mark_cnt = (rand() % marks_per_test) + 1;

    for (size_t m = 0; m < mark_cnt; m++) {
      size_t pos = rand() % tab_size;
      size_t len = (rand() % (tab_size - pos) / 4 + 1);
      int val = rand() % levels;
      //std::cout << "mark " << pos << "~" << len << " " << val << std::endl;
      S.mark(SstCheck::BlockHandle{pos, len, val});
      for (size_t i = pos; i < pos + len ; i++) {
	tab[i] = val;
      }  
    }

    for (auto& it : S.use_cf_map) {
      //std::cout << "[" << it.first << "~" << it.second.len << " " << it.second.cf << std::endl;
    }
    
    for (auto& it : S.use_cf_map) {
      for (size_t i = it.first; i < it.first + it.second.len; i++) {
	ASSERT_EQ(tab[i], it.second.cf);
      }
    }

    //find minimal number of regions needed
    int prev = -1;
    size_t cnt = 0;
    for (size_t i = 0 ; i < tab_size; i++) {
      if (tab[i] != -1 && tab[i] != prev) {
	cnt++;
      }
      prev = tab[i];
    }

    ASSERT_EQ(cnt, S.use_cf_map.size());
  }
  
}

TEST(TestSstCheck, mkfs) {
  /*
  uint64_t size = 1048576 * 128;
  TempBdev bdev{size};
  uuid_d fsid;
  BlueFS fs(g_ceph_context);
  ASSERT_EQ(0, fs.add_block_device(BlueFS::BDEV_DB, bdev.path, false, 1048576));
  ASSERT_EQ(0, fs.mkfs(fsid, { BlueFS::BDEV_DB, false, false }));
  */
  std::cout << test_surveyor_data[0].desc << std::endl;
  std::cout << "x" << test_surveyor_data[0].bytes <<
	    "x" << std::endl;
  
  SstCheck S(g_ceph_context);

  char* tab = (char*)malloc(test_surveyor_data[0].size);
  memcpy(tab, test_surveyor_data[0].bytes, test_surveyor_data[0].size);
  //memset(tab + 1000, '-', 30);
  S.init(test_surveyor_data[0].size, (const uint8_t*)tab);
  //  S.init(test_surveyor_data[0].size, (const uint8_t*)test_surveyor_data[0].bytes);
  S.examine();
  dout(10);
  S.dump_cf_map(*_dout);
  *_dout << dendl;
#if 0
  std::cout << S.read_footer() << std::endl;
  std::cout << S.footer.metaindex.pos << std::endl;
  std::cout << S.footer.metaindex.len << std::endl;
  std::cout << S.footer.metaindex.cf << std::endl;
  std::cout << S.footer.index.pos << std::endl;
  std::cout << S.footer.index.len << std::endl;
  std::cout << S.footer.index.cf << std::endl;

  SstCheck::CF cf;
  SstCheck::Index index;
  cf = S.read_KV_index(S.footer.index.pos,
		       S.footer.index.pos + S.footer.index.len,
		       false,
		       index);

  std::cout << "kvindex=" << cf << std::endl;
  std::cout << index.cnt.v << std::endl;
  std::cout << index.cnt.cf << std::endl;
  for (auto& p : index.idx) {
    std::cout << p.v << std::endl;
    std::cout << p.cf << std::endl;
  }

  std::vector<SstCheck::KB> blockkeys;
  cf = S.read_KB_values(S.footer.index.pos,
			S.footer.index.pos + S.footer.index.len,
			SstCheck::BlockDataIndex,
			index,
			blockkeys);
  std::cout << "kb=" << cf << std::endl;
  std::cout << index.cnt.v << std::endl;
  std::cout << index.cnt.cf << std::endl;
  for (auto& p : blockkeys) {
    std::cout << pretty_binary_string(p.name) << std::endl;
    std::cout << p.value.pos << ":" << p.value.len << "(" << p.value.cf << ")" << std::endl;
    std::cout << p.supp.pos << ":" << p.supp.len << "(" << p.supp.cf << ")" << std::endl;
    S.check_crc(p.value.pos, p.value.len);
    
    {
      SstCheck::Index index;
      cf = S.read_KV_index(p.value.pos, p.value.pos + p.value.len,
			   true,
			   index);
      std::cout << "index for data block cf=" << cf << std::endl;
      for (auto& r: index.idx) {
	std::cout << "r " << r.v << std::endl;
	std::cout << "r " << r.cf << std::endl;
      }
      std::vector<SstCheck::KV> kvs;
      cf = S.read_KV_values(p.value.pos, p.value.pos + p.value.len,
			    index,
			    kvs);
      for (auto& s: kvs) {
	std::cout << pretty_binary_string(s.name) << std::endl;
	//	std::cout << pretty_binary_string(s.name) << std::endl;	
      }
      
    }

    
  }



  
  
  {
  SstCheck::Index index;
  cf = S.read_KV_index(S.footer.metaindex.pos,
		       S.footer.metaindex.pos + S.footer.metaindex.len,
		       false,
		       index);

  std::cout << "kvindex=" << cf << std::endl;
  std::cout << index.cnt.v << std::endl;
  std::cout << index.cnt.cf << std::endl;
  for (auto& p : index.idx) {
    std::cout << p.v << std::endl;
    std::cout << p.cf << std::endl;
  }
  }
  #endif
}


int main(int argc, char **argv) {
  auto args = argv_to_vec(argc, argv);
  map<string,string> defaults = {
    { "debug_bluefs", "1/20" },
    { "debug_bdev", "1/20" }
  };

  auto cct = global_init(&defaults, args, CEPH_ENTITY_TYPE_CLIENT,
			 CODE_ENVIRONMENT_UTILITY,
			 CINIT_FLAG_NO_DEFAULT_CONFIG_FILE);
  common_init_finish(g_ceph_context);
  g_ceph_context->_conf.set_val(
    "enable_experimental_unrecoverable_data_corrupting_features",
    "*");
  g_ceph_context->_conf.apply_changes(nullptr);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
