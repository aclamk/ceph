// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
#ifndef CEPH_OS_BLUESTORE_SSTCHECK_H
#define CEPH_OS_BLUESTORE_SSTCHECK_H

//#include "BlueFS.h"

//#include "common/debug.h"
//#include "common/errno.h"
//#include "common/perf_counters.h"
//#include "include/ceph_assert.h"
#include "common/ceph_context.h"
#include <string>
#include <inttypes.h>

class SstCheck {
public:
  // sets examined sst file
  void init(size_t sst_size, const uint8_t* sst_data);
  SstCheck(CephContext* cct)
    : cct(cct) {};

  typedef float CF;

  struct BlockHandle {
    uint32_t pos;
    uint32_t len;
    CF cf;
    BlockHandle(uint32_t pos, uint32_t len, CF cf)
      : pos(pos), len(len), cf(cf) {}
    BlockHandle()
      : pos(0), len(0), cf(0) {};
    friend std::ostream& operator<<(std::ostream& out, const BlockHandle& bh) {
      out << "{0x" << std::hex << bh.pos << "~" << bh.len << std::dec;
      if (bh.cf != 1) {
	out << "!" << bh.cf;
      }
      out << "}";
      return out;
    }

  };


  
private:
  CephContext* cct;
  // read content of sst file to buffer
  // this is the only function that reads from device
  //  void read_sst();

public:
  // recover data from bluefs cache
  void peek_bluefs_buffers();

  //void scan_blocks();

  // attempts to read footer, if successfull, will add blocks
  CF read_footer();

  // read meta
  int read_meta();

  // read index
  int read_index();

  // read blocks from provided list
  // modifies ok_map
  int read_blocks();


  // attempts to recover data from block
  // crawls through data in block attemp
  int scan_block_data();

  
  // attempts to read keys starting from specified position
  // it will continue if decoding is consistent enough
  // abort if fundamental boundaries are broken
  // it might modify ok_map if sufficient confidence level is reached
  //
  // will observe hard boundaries of already valid blocks
  int attempt_keys();


  // report confidence map
  // this maps regions in file to specific probability of being corrupted
  int get_ok_map();

  // return map of already identified blocks
  int block_map();

  //  double read_KV(size_t offset, prev_name, name, value_size);


  uint32_t calc_crc(uint32_t pos, uint32_t len);
  bool check_crc(uint32_t block_pos, uint32_t block_len);
  
  // section for accessing sst data
  uint32_t read_u32(uint32_t pos);
  ssize_t read_varint(uint32_t pos, uint64_t& var);


  
  
  struct Block {
    size_t start;
    size_t len;
    char type; //k-v, other
    char state; //unknown, ok, failed-crc
    CF cf; //confidence;
  };

#if 0
  
  // represents single K-V pair in some place inside block
  struct KV {
    std::string name;
    BlockHandle value_at; //where value is defined (cf is unused here)
    BlockHandle supp; //support; range inside sst file that defines this key //TODO - maybe it should be inside block?
  };

  // Represents keys that encode blocks on disk. Value for them is always 2 varints, offset + size.
  // Used in Index block.
  struct KB {
    std::string name; //name of key
    BlockHandle value; //value = definition of block
    BlockHandle location; //where is this key defined, in file
  };
#endif
  
  struct KD {
    std::string name; // name of key
    BlockHandle supp; // support; range inside sst file that defines this key //TODO - maybe it should be inside block?
  };

  // This variant of Key-Value mapping is used by RocksDB to maintain block indexes inside sst files.
  // Value of key is always pair of varints, and size of value is not encoded.
  struct KB : public KD {
    BlockHandle value; // Contains value of key decoded as pair of varints.
                       // If values are outside accepted range or any other problem exists, cf is low.
  };

  // This type is used for data block, to represent actual DB keys
  struct KV : public KD {
    BlockHandle value_at; // Contains range where bytes of value are located. cf = supp.cf
  };

   
    
  
  KB read_KB(uint32_t pos);


  struct uint32_cf {
    uint32_t v = 0; // position relative to beginning of block
    CF cf = 0;    // confidence that value is correct
    uint32_cf(uint32_t v, CF cf)
      : v(v), cf(cf) {}
    uint32_cf()
      : v(0), cf(0) {}
  };
  
  struct Off {
    uint32_t off = 0; // position relative to beginning of block
    CF cf = 0;    // confidence that value is correct
  };

  struct Index {
    uint32_cf cnt;
    std::vector<uint32_cf> idx;
    BlockHandle supp;
  };

  enum BlockType
    {
     BlockUnknown,
     BlockData,
     BlockDataIndex,
     BlockMetaIndex
    };

  struct {
    uint32_cf start; // points to first byte of block currently parsed
    uint32_cf end;   // points to byte "compressed?" right before checksum
    BlockType type;       //
  } curr_block;

    
  // represents block that encodes k-v data
  struct KVBlock : public Block {
    //Key[] index_keys;
    std::vector<Off> index_offs;
  };

  KV read(KVBlock& kvb,
	  size_t offset,
	  const std::string& prev_name,
	  std::string& name,
	  size_t value_size,
	  double p_cf);

  // reads KVBlock 
  KVBlock read(KVBlock& kvb,
	       double p_cf);



  CF read_index_array(uint32_t b_start, // points to first byte of block
		      uint32_t b_end,   // points to byte "compressed?" right before checksum
		      bool keys_with_length, //true = normal keys with length specified after key length
		                             //false = keys that encode pair of varint, no length specified
		      Index& index);

  template<typename K>
  K read(const K *prev_key,
	 uint32_t pos);


  // uses internally curr_block
  template<typename K>
  CF read_keyvalue_section(const Index& index,     // index to hint and actually match values
			   std::vector<K>& kvs);
  template<typename K>
  CF keyvalue_section_read_best(const K* prev_key,
				uint32_t start_min,
				uint32_t start_max,
				uint32_t limit,
				std::vector<K>& kvs);
  
  // reads Key->Block values from well defined block
  CF read_KB_values(uint32_t b_start, // points to first byte of block
		    uint32_t b_end,   // points to byte "compressed?" right before checksum
		    BlockType type,   // type of block BlockDataIndex or BlockMetaIndex
		    const Index& index,     // index to hint and actually match values
		    std::vector<KB>& blockkeys);

  // reads Key->Value from well defined block
  // this reads actual keys
  CF read_KV_values(uint32_t b_start, // points to first byte of block
		    uint32_t b_end,   // points to byte "compressed?" right before checksum
		    const Index& index,     // index to hint and actually match values
		    std::vector<KV>& kvs);

  #if 0
  // reads Key->Value entries from well defined block
  // this is used for Data Blocks
  CF read_KV_values(uint32_t b_start, // points to first byte of block
		    uint32_t b_end,   // points to byte "compressed?" right before checksum
		    const Index& index,     // index to hint and actually match values
		    std::vector<KV>& kvs);
#endif		    
  
  // reads index for K-V blocks
  // 'from' is always fixed
  // 'to' is with some confidence
  // if CF>=1 never attempt to extent range
  // 
  void read_KV_index_backwards(uint32_t from_low,  //range to 
			  uint32_t from_high,
			  uint32_t to,        //location of 'compressed' byte
			  double to_CF);

  template<typename K>
  CF expect_ref_key(uint32_t pos);
  //		    bool keys_with_length //true = normal keys with length specified after key length
  //		    //false = keys that encode pair of varint, no length specified
  //		    );
  

  CF expect_ref_key(uint32_t pos,
		    bool keys_with_length //true = normal keys with length specified after key length
		    //false = keys that encode pair of varint, no length specified
		    );
  
  int examine();
  
  // attempt to fully cover file with known blocks
  // will start from footer and indexes,
  // but if it fails, attempts to scan file from top
  int scan_blocks();



  struct cf_region {
    uint32_t len;
    CF cf;
  };

  void punch_hole(const BlockHandle& bh);
  void mark(const BlockHandle& bh);
  void dump_cf_map(std::ostream& out);

  // Map of regions <begin...end, cf>.
  // Key in map is end of region. Overlap is illegal.
  std::map<uint32_t, cf_region> use_cf_map;
  
  size_t sst_size;
  const uint8_t* sst_data;

  struct {
    BlockHandle index;
    BlockHandle metaindex;
    BlockHandle supp;
  } footer;

  Index data_index;
  Index meta_index;
  std::vector<KB> data_blocks_keys;
  // meta blocks are defined with length, even though it contains Block Handle
  // converted to KB during read
  std::vector<KB> meta_blocks_keys;
};


#endif
