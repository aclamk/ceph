// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "SstCheck.h"


#include "BlueFS.h"

#include "common/debug.h"
#include "common/errno.h"
#include "common/perf_counters.h"
#include "include/ceph_assert.h"
#include "common/pretty_binary.h"

#define dout_context cct
#define dout_subsys ceph_subsys_bluefs
#undef dout_prefix
#define dout_prefix *_dout << "sst "





// Examine SST file for possible corruption
// 
void BlueFS::SstSurvey(const std::string& sst)
{
  SstCheck S(cct);

  //  S.examine(sst);
}

int Survey(size_t sst_size, const uint8_t* sst_data)
{
  SstCheck S(g_ceph_context);
  //  S.init(sst_size, sst_data);
  // S.read_footer();

  return 0;
}


void SstCheck::init(size_t sst_size, const uint8_t* sst_data)
{
  this->sst_size = sst_size;
  this->sst_data = sst_data;
}



uint32_t SstCheck::read_u32(uint32_t pos)
{
  ceph_assert(pos + 3 < sst_size);
  uint32_t res =
    (uint32_t)sst_data[pos]         |
    (uint32_t)sst_data[pos+1] << 8  |
    (uint32_t)sst_data[pos+2] << 16 |
    (uint32_t)sst_data[pos+3] << 24;
  return res;
}

// function reads varint, but never overshoots end of sst
ssize_t SstCheck::read_varint(uint32_t pos, uint64_t& var)
{
  //FIXME! assert, really?
  ceph_assert(pos < sst_size);
  int max_bits = std::min<uint64_t>(64, (sst_size - pos) * 7);  
  int bits = 0;
  uint32_t i = pos;
  bool more;
  uint64_t v = 0;
  do {
    v |= (sst_data[i] & 0x7f) << bits;
    more = sst_data[i] & 0x80;
    bits += 7;
    i++;
  } while(more && bits < max_bits);
  var = v;
  dout(30) << __func__ << " @" << pos << "=" << v << dendl;
  return i - pos;
}


template<>
SstCheck::CF SstCheck::expect_ref_key<SstCheck::KB>(uint32_t _pos)				 
{
  uint32_t pos = _pos;
  int l;
  uint64_t reused_key;
  l = read_varint(pos, reused_key);
  pos += l;
  // ref keys are not allowed to reuse from previous keys
  if (reused_key != 0)
    return 0;
  uint64_t new_key;
  l = read_varint(pos, new_key);
  pos += l;

  //BETTER restrict to current block
  if (pos + new_key > sst_size)
    return 0;

  //BETTER we could somehow check value of the key, or read next, to compare if ascending
  pos += new_key;
  // now it should be possible to read 2 varints encoding range inside current block

  uint64_t ref_pos;
  uint64_t ref_len;
  l = read_varint(pos, ref_pos);
  pos += l;
  l = read_varint(pos, ref_len);
  pos += l;

  //BETTER limit more
  if (pos > sst_size)
    return 0;

  // cannot refer future
  if (ref_pos > _pos)
    return 0;
  
  if (ref_pos + ref_len > _pos)
    return 0;
  //BETTER ref_pos and ref_len could use check for being realistic values
  return 1;
}

template<>
SstCheck::CF SstCheck::expect_ref_key<SstCheck::KV>(uint32_t _pos)				 
{
  uint32_t pos = _pos;
  int l;
  uint64_t reused_key;
  l = read_varint(pos, reused_key);
  pos += l;
  // ref keys are not allowed to reuse from previous keys
  if (reused_key != 0)
    return 0;
  uint64_t new_key;
  l = read_varint(pos, new_key);
  pos += l;

  uint64_t value_size;
  l = read_varint(pos, value_size);
  pos += l;
  
  //BETTER restrict to current block
  if (pos + new_key > sst_size)
    return 0;

  //BETTER we could somehow check value of the key, or read next, to compare if ascending
  pos += new_key;
  // now it should be possible to read 2 varints encoding range inside current block

  pos += value_size;

  //BETTER limit more
  if (pos > sst_size)
    return 0;

  return 1;
}



namespace rocksdb {
namespace crc32c {
extern uint32_t Extend(uint32_t init_crc, const char* data, size_t n);
}
}
uint32_t SstCheck::calc_crc(uint32_t pos, uint32_t len)
{
  //return ceph_crc32c(0, sst_data + pos, len);
  return rocksdb::crc32c::Extend(0, (const char*)sst_data + pos, len);
}

// <copied from RocksDB/crc32c.h>
static const uint32_t kMaskDelta = 0xa282ead8ul;

// Return a masked representation of crc.
//
// Motivation: it is problematic to compute the CRC of a string that
// contains embedded CRCs.  Therefore we recommend that CRCs stored
// somewhere (e.g., in files) should be masked before being stored.
inline uint32_t Mask(uint32_t crc) {
  // Rotate right by 15 bits and add a constant.
  return ((crc >> 15) | (crc << 17)) + kMaskDelta;
}

// Return the crc whose masked representation is masked_crc.
inline uint32_t Unmask(uint32_t masked_crc) {
  uint32_t rot = masked_crc - kMaskDelta;
  return ((rot >> 17) | (rot << 15));
}
// </copied from RocksDB/crc32c.h>


bool SstCheck::check_crc(uint32_t block_pos, uint32_t block_len)
{
  uint32_t crc = calc_crc(block_pos, block_len + 1);
  uint32_t crc_read = read_u32(block_pos + block_len + 1);
  dout(0) << "crcs: " << std::hex << crc << " " << crc_read << " " << std::dec << dendl;
  crc_read = Unmask(crc_read);
  dout(0) << "crcs: " << std::hex << crc << " " << crc_read << " " << std::dec << dendl;
  return crc == crc_read;
}

// BETTER need more params to limit forward-range jumps
// KB is used for 2 purposes
// 1. in data index table; values are monotonic, ~4K in size, and define Data Blocks, starting from 0
// 2. in meta index tables; no location limit, no size limit

template<>
SstCheck::KB SstCheck::read<SstCheck::KB>(const KB *prev_key,
					  uint32_t _pos)
{
  KB kb;
  uint32_t pos = _pos;
  int l;
  uint64_t reused_key;
  ceph_assert(curr_block.type == BlockDataIndex || curr_block.type == BlockMetaIndex);
  ceph_assert(curr_block.end.v > curr_block.start.v);
  ceph_assert(pos < curr_block.end.v);
  
  l = read_varint(pos, reused_key);
  pos += l;
  // ref keys are not allowed to reuse from previous keys
  // all KB keys are ref keys
  if (reused_key != 0) {
    // not even one byte accepted
    kb.supp = BlockHandle(_pos, 0, 0.05);
    return kb;
  }
  uint64_t new_key;
  l = read_varint(pos, new_key);
  pos += l;
  
  if (new_key == 0) {
    // key cannot be zero-length
    kb.supp = BlockHandle(_pos, pos-_pos, 0.06);
    return kb;
  }
  //  std::cout << "new_key=" << new_key << std::endl;;

  uint64_t val_size;
  if (curr_block.type == BlockMetaIndex) {
    l = read_varint(pos, val_size);
    pos += l;
  }
  //BETTER restrict to current block
  if (pos + new_key > sst_size) {
    // but if key extends beyond sst_size we cannot construct key at all
    // MAYBE confidence on sst_size is 1, therefore confidence is 0
    kb.supp = BlockHandle(_pos, pos-_pos, 0.07);
    return kb;
  }
  
  kb.name = std::string((const char*) sst_data + pos, new_key);
  pos += new_key;
  // now it should be possible to read 2 varints encoding range inside current block

  uint64_t ref_pos;
  uint64_t ref_len;

  uint64_t pos_lock = pos;
  l = read_varint(pos, ref_pos);
  pos += l;
  l = read_varint(pos, ref_len);
  pos += l;

  if (curr_block.type == BlockMetaIndex) {
    // TODO size of value must be equal to declared size
    val_size == pos - pos_lock;
  }
  
  // here we read everyting that we could from key
  // now analize content
  
  CF supp_cf = 1; // initially we assume support is ok
  if (pos > curr_block.end.v) {
    // we did overshoot end of block
    // the more confidence we had in range, the less confidence we have in support
    supp_cf = 0.9 - curr_block.end.cf;
  }

  CF value_cf = 1;
  // now checking quality of Block definition
  if (ref_pos > _pos) {
    // key must define block earlier in file
    value_cf = 0;
    kb.supp = BlockHandle(_pos, pos - _pos, 0.01);
    return kb;
  }
  
  if (ref_pos + ref_len > _pos) { //MAGIC 5
    // defined block must not overlap with key
    value_cf = 0;
    kb.supp = BlockHandle(_pos, pos - _pos, 0.02);
    return kb;
  }

  if (curr_block.type == BlockDataIndex) {
    // keys in data index block are monotonic
    if (prev_key == nullptr) {
      if (ref_pos != 0) {
	// cf penalty for non-0 start
	value_cf = value_cf - 0.8; //TODO find proper val, proper operation
      }
    } else {
      uint32_t pk_end = prev_key->value.pos + prev_key->value.len + 5; //TODO magic 5
      if (ref_pos < pk_end) {
	// 
	value_cf = value_cf - 0.6;
      } else if (ref_pos > pk_end) {
	// BETTER make cf drop dependent on pk_end-ref_pos
	value_cf = value_cf - 0.2;
      }
    }
    // size is expected to be ~4K
    // TODO get value from RocksDB config
    float size_f = ref_len / 4096;
    if (size_f > 1) value_cf -= 0.1;
    if (size_f > 2) value_cf -= 0.1;
    if (size_f > 4) value_cf -= 0.1;
    if (size_f > 8) value_cf -= 0.1;
  }
  //MAYBE - crc region ?
  
  kb.value = BlockHandle(ref_pos, ref_len, value_cf);  
  kb.supp = BlockHandle(_pos, pos - _pos, supp_cf);
  return kb;
}

template<>
SstCheck::KV SstCheck::read<SstCheck::KV>(const KV *prev_key, uint32_t _pos)
{
  KV kv;
  uint32_t pos = _pos;
  int l;
  uint64_t reused_size;
  l = read_varint(pos, reused_size);
  pos += l;

  CF key_cf = 1;
  CF prev_cf;
  dout(29) << reused_size << "/" << curr_block.end.v - _pos << dendl;
  if (reused_size > curr_block.end.v - _pos) {
    reused_size = 0;
    key_cf = 0.01;
  }
  
  
  if (prev_key == nullptr) {
    prev_cf = 1;
    // no previous key or previous key too corrupted(maybe)
    if (reused_size == 0) {
      // sun shining
    } else {
      key_cf -= 0.4;
      // MAYBE - is there some value in guessing what proportion of reused_size/new_size is normalest?
      kv.name = std::string(reused_size, '\0');
    }
  } else {
    prev_cf = prev_key->supp.cf;
    // we have a key to base on  
    if (prev_key->name.length() >= reused_size) {
      kv.name = prev_key->name.substr(0, reused_size);
    } else {
      // cannot reuse chars that do not exist
      // copy as much as we could and fill with zeros
      kv.name = prev_key->name + std::string(reused_size - prev_key->name.length(), '\0');
      key_cf -= 0.2;
    }
  }

  uint64_t new_size;
  l = read_varint(pos, new_size);
  pos += l;

  if (new_size == 0) {
    // new part of key cannot be zero-length, it would give two exactly the same keys
    kv.supp = BlockHandle(_pos, pos-_pos, 0.01);
    return kv;
  }
  if (new_size > 100000000) {
    // BETTER find nicer limit
    kv.supp = BlockHandle(_pos, pos-_pos, 0.03);
    return kv;
  }
  // key must not go outside current block
  if (pos + new_size > curr_block.end.v) {
    kv.supp = BlockHandle(_pos, pos-_pos, 0.02);
    return kv;
  }
  uint64_t value_size;
  l = read_varint(pos, value_size);
  pos += l;

  dout(25) << __func__ << "reu=" << reused_size << " new=" << new_size << " val=" << value_size << dendl;
  
  if (pos + value_size > curr_block.end.v) {
    kv.supp = BlockHandle(_pos, pos-_pos, 0.03);
    return kv;
  }

    // new part of key
  kv.name += std::string((const char*) sst_data + pos, new_size);
  pos += new_size;
  

  kv.value_at = BlockHandle(pos, value_size, key_cf);
  pos += value_size;
  //  std::cout << "old=" << reused_size << " new=" << new_size << " value_size=" << value_size << std::endl;
  if (key_cf >= prev_cf) {
    key_cf = prev_cf * 0.9 + key_cf * 0.1;
  } else {
    key_cf = prev_cf * 0.5 + key_cf * 0.5;
  }
  kv.supp = BlockHandle(_pos, pos - _pos, key_cf);
  return kv;
}


void SstCheck::punch_hole(const BlockHandle& bh)
{
  auto it = use_cf_map.lower_bound(bh.pos);
  if (it != use_cf_map.begin()) {
    // it is not first element previous exists
    --it;
  }
  uint32_t bh_pos = bh.pos;
  uint32_t bh_end = bh.pos + bh.len;

  while (it != use_cf_map.end() && it->first < bh_end) {
    uint32_t it_pos = it->first;
    uint32_t it_end = it->first + it->second.len;
    CF it_cf = it->second.cf;
    if (it_end <= bh_pos || it_pos >= bh_end) {
      // no overlap
      ++it;
      continue;
    }

    if (it_pos < bh_pos && bh_end < it_end) {
      // [    it    )  ->  [it)  [it)
      //    [ bh )            [bh)
      it->second.len = bh_pos - it_pos;
      it = use_cf_map.insert(it, {bh_end, cf_region{it_end - bh_end, it_cf}});
      ++it;
      continue;
    }

    if (it_pos >= bh_pos && it_end <= bh_end) {
      //   [ it )    ->  
      // [   bh   )      [   bh   )
      it = use_cf_map.erase(it);
      continue;
    }
  
    if (it_pos < bh_pos && bh_pos <= it_end) {
      // [ it )     ->  [it)
      //    [ bh )         [ bh )
      it->second.len = bh_pos - it_pos;
      ++it;
      continue;
    }

    if (bh_pos <= it_pos && it_pos < bh_end) {
      //    [ it )  ->       [it)
      // [ bh )         [ bh )
      it = use_cf_map.erase(it);
      it = use_cf_map.insert(it, {bh_end, cf_region{it_end - bh_end, it_cf}});
      ++it;
      continue;
    }
    ceph_assert(0 && "conditions should be exhaustive");
  }
}
  
void SstCheck::mark(const BlockHandle& bh)
{
  dout(27) << __func__ << " " << bh << dendl;
  punch_hole(bh);
  auto it = use_cf_map.emplace(bh.pos, cf_region{bh.len, bh.cf}).first;

  if (it != use_cf_map.begin()) {
    --it;
  }
  auto next_it = it;
  ++next_it;

  while (next_it != use_cf_map.end() && next_it->first <= bh.pos + bh.len) {
    ceph_assert(it->first + it->second.len <= next_it->first);
    if (it->second.cf == next_it->second.cf &&
	it->first + it->second.len == next_it->first) {
      it->second.len += next_it->second.len;
      next_it = use_cf_map.erase(next_it);
    } else {
      ++it;
      ++next_it;
    }
  }

  dout(27) << __func__ << " dump:";
  dump_cf_map(*_dout);
  *_dout << dendl;
}

void SstCheck::dump_cf_map(std::ostream& out)
{
  for (auto& it : use_cf_map) {
    out << "{" << std::hex << it.first << "-" << it.first + it.second.len << std::dec << " " << it.second.cf << "} ";
  }
}






// attempt to fully cover file with known blocks
// will start from footer and indexes,
// but if it fails, attempts to scan file from top
#if 0
int SstCheck::scan_blocks()
{
  int r;
  // attempt to read footer
  r = read_footer();
  if (r == 0) {
    if (footer.index) {
      if (check_block_crc(footer.index)) {
	// crc is ok

	// verify consistency of index block, it is similar to kv but without size,
	// as its value is always BlockHandle
	check_block_kv(footer.index, true/*special, with fixed values*/);
	// this will produce blocks with basic high confidence
      } else {
	// crc broken, but for k-v tables we can try to narrow down failure range
	scan_block_kv(footer.index, true/*special, with fixed values*/);
	// this might produce blocks with low confidence
	
      }
    }
    // if some of footer.index checking fails
    footer.metaindex;

  }
}
#endif

// 1. beginning + end known, data ok
// 2.                        data corrupted
// 3. beginning known, end unknown <- this is when we scan from beginning of sst, or after any well recovered block
// 4. end known, beginning unknown <- this is when we backtrack from next valid block


/* 
   Simple version of reading index table.
   Start and end are fixed.
   We still assume data might be corrupted.
 */
SstCheck::CF SstCheck::read_index_array(uint32_t b_start, // points to first byte of block
					uint32_t b_end,   // points to byte "compressed?" right before checksum
					bool keys_with_length, //true = normal keys with length specified after key length
					                       //false = keys that encode pair of varint, no length specified
					Index& index)
{
  index.supp = BlockHandle{0, 0, 0};
  //plain deterministic read
  uint32_t cnt = read_u32(b_end - 4);

  // evaluate cnt - it cannot be too big
  if (b_start + cnt * (1 + 1 + 1 + 1) /*shortest ref key entry*/ +
                cnt * 4 + 4 /*index table size*/ > b_end) {
    // cannot trust cnt at all

    // maybe just read some entries backwards to check if match ref
    // nah, nearby indexes likely be corrupted too
    return 0;
  }
  uint32_t tpos = b_end - (cnt * 4 + 4); // position of index table

  index.cnt = uint32_cf{cnt, 1};
  auto& idx = index.idx;
  CF cf;
  // first element must be 0

  for (uint32_t i = 0; i < cnt; i++) {
    uint32_t v = read_u32(tpos + i * 4);
    if (i == 0) {
      // first entry must be zero
      cf = v == 0 ? 1 : 0;
    } else {
      if (b_start + v > tpos) {
	cf = 0;
      } else {
	if (keys_with_length) {
	  cf = expect_ref_key<KV>(b_start + v) * 0.5 + 0.5;
	} else {
	  cf = expect_ref_key<KB>(b_start + v) * 0.5 + 0.5;
	}
      }
    }
    idx.emplace_back(v, cf);
  }
  // idx now has elements. now evaluate them
  // 1. can be corroborated as ref keys?
  // 2. are monotonic? select longer monotonic sequence
  // or just select something...

  // start from back. more chance data around cnt uncorrupted
  uint32_t last = tpos - b_start;
  for (int i = idx.size() - 1; i >= 0; i--) {
    if (idx[i].cf > 0) {
      if (idx[i].v < last) {
	// accept this to monotonic chain
	last = idx[i].v;
      } else {
	// reject
	idx[i].cf = 0; //todo maybe leave some confidence?
      }
    }
  }
  index.supp = BlockHandle{tpos, b_end - tpos, 1};
  return 1; //todo, it really should depend on compound confidence
}



// Reads Key->Block values from well defined block
// Keys in index are all full-defined (0 reuse length) and each
// of them is listed in index table at the end of block.
template<typename K>
SstCheck::CF SstCheck::read_keyvalue_section(
					     //    uint32_t b_start, // points to first byte of block
					     //    uint32_t b_end,   // points to byte "compressed?" right before checksum
    const Index& index,     // index to hint and actually match values
    std::vector<K>& blockkeys)
{
  dout(20) << " reading keys " << std::hex << curr_block.start.v << " to " << curr_block.end.v << std::dec << dendl;
  // If index is high cf then stop reading keys at beginning of index.
  // Otherwise try until: pos + #keys * 4 + 4 >= b_end

  // BETTER improve to scan keys and cross-check with index
  K* prev_key = nullptr;
  uint32_t pos = curr_block.start.v;
  std::vector<K> hold;
  do {
    K k = read<K>(prev_key, pos);
    //hold.push_back();
    dout(25) << "key="  << pretty_binary_string(k.name) << " supp=" << k.supp << dendl;
    //    std::cout << "key name=" << pretty_binary_string(k.name) << " supp=" << k.supp << std::endl;
    if (k.supp.cf > 0.9) {
      pos += k.supp.len;
      blockkeys.push_back(k);
      prev_key = &blockkeys.back();
    } else {
      // crawl ahead
      keyvalue_section_read_best(prev_key, pos, pos + 100, pos + 1000, hold);
      //for (auto& h : hold) {
      //mark(h.supp);
      //}
      blockkeys.insert(blockkeys.end(), hold.begin(), hold.end());
      pos = curr_block.start.v + hold.back().supp.pos + hold.back().supp.len;
      prev_key = &blockkeys.back();
      //mark(k.supp);
      //pos++;
    }
    //  } while (pos + blockkeys.size() * 4 + 4 < curr_block.end.v);
  } while (pos < index.supp.pos);
  
  return 1;
}

template<typename K>
SstCheck::CF SstCheck::keyvalue_section_read_best(
						  const K* prev_key,
						  uint32_t start_min,
						  uint32_t start_max,
						  uint32_t limit,
						  std::vector<K>& blockkeys
						  )
{
  dout(15) << __func__
	   << " start_min=" << start_min
	   << " start_max=" << start_max
	   << " limit=" << limit << dendl;
  std::vector<std::vector<K>> chains;

#if 0
  // initial fill
  chains.resize(start_max - start_min);
  for (uint32_t pos = start_min; pos < start_max; pos++) {
    chains[pos - start_min].push_back(read<K>(prev_key, pos));
  }
  //each key that was read jump
#endif

  for (uint32_t checkpoint = start_min; checkpoint < limit; checkpoint++) {
    if (checkpoint < start_max) {
      // start new chain
      dout(20) << __func__ << " adding chain " << checkpoint << dendl;
      std::vector<K> chain;
      K k = read<K>(prev_key, checkpoint);
      chain.push_back(k);
      dout(22) << __func__ << " chain " << k.supp.pos
	       << " key=" << pretty_binary_string(k.name) << " supp=" << k.supp << dendl;
      
      // now find a position to insert new chain
      auto it = chains.begin();
      while (it != chains.end() &&
	     (*it).back().supp.pos + (*it).back().supp.len <
	     chain.back().supp.pos + chain.back().supp.len) {
	++it;
      }
      chains.insert(it, chain);
    }

    // process all ready chains
    auto it = chains.begin();
    while (it != chains.end() &&
	   (*it).back().supp.pos + (*it).back().supp.len <= checkpoint) {
      ceph_assert((*it).back().supp.pos + (*it).back().supp.len == checkpoint);
      // this chain is ready for adding new key
      K k = read<K>(&(*it).back(), checkpoint);
      (*it).push_back(k);
      dout(22) << __func__ << " chain " << (*it).front().supp.pos
	       << " key=" << pretty_binary_string(k.name) << " supp=" << k.supp << dendl;
      ++it;
    }
    if (it != chains.begin()) {
      // something changed, sort
      dout(25) << __func__ << " sorting checkpoint=" << checkpoint << dendl;
      std::sort(chains.begin(), chains.end(),
		[](const std::vector<K> &a, const std::vector<K> &b) {
		  return
		    a.back().supp.pos + a.back().supp.len <
		    b.back().supp.pos + b.back().supp.len;
		});
      // attempt to merge chains
      // merge chains if pos are the same
      for (size_t m = 0; m < chains.size() - 1; /*conditional ++m*/) {
	if (chains[m].back().supp.pos + chains[m].back().supp.len ==
	    chains[m+1].back().supp.pos + chains[m+1].back().supp.len) {
	  size_t s0 = chains[m].size();
	  size_t s1 = chains[m+1].size();
	  CF score0 = chains[m].back().supp.cf + s0 * 0.03;
	  CF score1 = chains[m+1].back().supp.cf + s1 * 0.03;
	  uint32_t ch0 = chains[m].front().supp.pos;
	  uint32_t ch1 = chains[m+1].front().supp.pos;
	  // BETTER need better function then average
	  if (score0 > score1) {
	    chains.erase(chains.begin() + (m+1));
	  } else {
	    chains.erase(chains.begin() + m);
	  }
	  dout(20) << __func__ << " chains " << ch0 << " and " << ch1
		   << " merged to " << chains[m].front().supp.pos << dendl;
	} else {
	  ++m;
	}
      }      
    } else {
      dout(25) << __func__ << " empty checkpoint=" << checkpoint << dendl;
    }

    for (auto& it : chains) {
      dout(26) << "chain " << (it).front().supp.pos << " size " << (it).size()
	       << " supp=" << (it).back().supp << dendl;
    }
  }
    
  bool cont;
  do {
    dout(20) << __func__ << " read chains iteration" << dendl;
    std::sort(chains.begin(), chains.end(),
	      [](const std::vector<K> &a, const std::vector<K> &b) {
		return
		  a.back().supp.pos + a.back().supp.len <
		  b.back().supp.pos + b.back().supp.len;
	      });
    cont = false;
    for (size_t i = 0; i < chains.size() / 2; i++) { //progress half that ends below median
      auto& c = chains[i];
      uint32_t pos = c.back().supp.pos + c.back().supp.len;
      if (pos < limit) {
	K k = read<K>(&c.back(), pos);
	c.push_back(k);
	cont = true;
	dout(22) << __func__ << " chain " << c.front().supp.pos
		 << " key=" << pretty_binary_string(k.name) << " supp=" << k.supp << dendl;
      }
    }
  } while (cont);
  dout(20) << __func__ << " finished" << dendl;
  // copy best chain out
  CF score_max = -1;
  size_t chain_max = 0;
  for (size_t i = 0; i < chains.size(); i++) {
    size_t s = chains[i].size();
    CF score = chains[i].back().supp.cf + s * 0.03;
    if (score > score_max) {
      chain_max = i;
      score_max = score;
    }
  }
  dout(22) << __func__<< " chain " << chains[chain_max].front().supp.pos << " selected " << dendl;
  blockkeys.swap(chains[chain_max]);
  return 1;
}



SstCheck::CF SstCheck::read_KB_values(
    uint32_t b_start, // points to first byte of block
    uint32_t b_end,   // points to byte "compressed?" right before checksum
    BlockType type,   // type of block BlockDataIndex or BlockMetaIndex
    const Index& index,     // index to hint and actually match values
    std::vector<KB>& blockkeys)
{
  dout(15) << __func__ << std::hex << " " << b_start << " to " << b_end << std::dec << dendl;
  curr_block.start = uint32_cf{b_start, 1};
  curr_block.end = uint32_cf{b_end, 1};
  curr_block.type = type;  
  return read_keyvalue_section<KB>(index, blockkeys);
}

SstCheck::CF SstCheck::read_KV_values(
    uint32_t b_start, // points to first byte of block
    uint32_t b_end,   // points to byte "compressed?" right before checksum
    const Index& index,     // index to hint and actually match values
    std::vector<KV>& blockkeys)
{
  dout(15) << __func__ << " " << std::hex << b_start << " to " << b_end << std::dec << dendl;
  curr_block.start = uint32_cf{b_start, 1};
  curr_block.end = uint32_cf{b_end, 1};
  curr_block.type = BlockData;
  return read_keyvalue_section<KV>(index, blockkeys);
}


//support; range inside sst file that defines this key //TODO - maybe it should be inside block?




#if 0
// reads Key->Value entries from well defined block
// this is used for Data Blocks
SstCheck::CF SstCheck::read_KV_values(uint32_t b_start, // points to first byte of block
				      uint32_t b_end,   // points to byte "compressed?" right before checksum
				      const Index& index,     // index to hint and actually match values
				      std::vector<KV>& kvs)
{
}
#endif



#if 0
// make a index table
int SstCheck::read_KV_index(uint32_t from,
			    double from_CF,
			    uint32_t to, //ideally, points to byte "compressed?" right before checksum
			    double to_CF)
{
  // if from.cf >= 1 and to.cf >=1 then do not attempt to search outside range

  if (from_CF >= 1 && to_CF >= 1) {
    //plain deterministic read
    uint32_t cnt = read_u32(to - 4);
    uint32_t tpos = to - 4 - cnt * 4; // position of index table

    // evaluate cnt - it cannot be too big
    if (cnt * (1 + 1 + 1 + 1)/*shortest ref key entry*/ < tpos - from) {
      // cannot trust cnt at all

      // maybe just read some entries backwards to check if match ref
      // nah, nearby indexes likely be corrupted too
      return -1;
    }
    // first element must be 0
    // read elements must be monotonic with diff >= 4

    for (int i = 0; i < cnt; i++) {
      uint32_t v = read_u32(tpos + i * 4);
      idx.push_back(v);
      if (i == 0) {
	// first entry must be zero
	cf = v == 0 ? 1 : 0;
      } else {
	if (v > tpos - from) {
	  cf = 0;
	} else {
	  cf = expect_ref_key(from + v);
	}
      }
      idx.emplace_back(v, cf);
    }
    // idx now has elements. now evaluate them
    // 1. can be corroborated as ref keys?
    // 2. are monotonic? select longer monotonic sequence
    // or just select something...

    // start from back. more chance data around cnt uncorrupted
    uint32_t last = tpos - from;
    for (int i = idx.size() - 1; i >= 0; i--) {
      if (idx[i].cf > 0) {
	if (idx[i].v < last) {
	  // accept this to monotonic chain
	  last = idx[i].v;
	} else {
	  // reject
	  idx[i].cf = 0; //todo maybe leave some confidence?
	}
      }
    }

#if 0    
    // if all indexes read are nicely monotonic, accept them
    std::vector<uint32> idx;
    uint32_t epos = to - 8;
    v = read_u32(epos);
    idx.push_back(v);
    // check sanity of v
    // v must be <0, to - from - cnt * 4>,
    // v should be in some proportion to ( to - from )

    // maybe also peek key here?
    // key must have "reused_name=0" and size that does make sense
    // ( for now refrain from checking if next after it is also a valid key )
    // ( additional corroboration would be that last key must end right before first index entry '0' )
    expect_ref_key(v);
    
    pos -= 4;
    while (true) {
      // perfect stop condition is if
      // we reached v == 0 and processed = cnt and the same time
      // if we reached v == 0 but still have more to process
      // then continue processing 
      uint32_t v = read_u32(pos);
      expect_ref_key(v);
      if (v < idx.back()) {
	// nice ordering, check proportion, check key
	
      }
    }
#endif

    
  }
  // backtrack table
  
}
#endif
static constexpr size_t kFooterSize = 40 + 4 + 8 + 1;
SstCheck::CF SstCheck::read_footer() {
  dout(20) << __func__ << dendl;
  // footer is always at the end
  // check bytes

  const uint8_t* end = sst_data + sst_size;
  int cb = 0; //count of incorrect bytes
  cb += end[-1] != 0x88;
  cb += end[-2] != 0xe2;
  cb += end[-3] != 0x41;
  cb += end[-4] != 0xb7;
  cb += end[-5] != 0x85;
  cb += end[-6] != 0xf4;
  cb += end[-7] != 0xcf;
  cb += end[-8] != 0xf7;
  cb += end[-9] != 0;
  cb += end[-10] != 0;
  cb += end[-11] != 0;
  cb += end[-12] != 4;
  
  // now decode 4 varints at -44
  int l;
  uint32_t pos = sst_size - kFooterSize + 1;
  uint32_t footer_start = pos;
  uint64_t index_pos;
  uint64_t index_size;
  uint64_t metaindex_pos;
  uint64_t metaindex_size;
  
  l = read_varint(pos, metaindex_pos);
  pos += l;
  l = read_varint(pos, metaindex_size);
  pos += l;
  l = read_varint(pos, index_pos);
  pos += l;
  l = read_varint(pos, index_size);
  pos += l;

  // the rest of bytes should be 0
  while (pos < sst_size - 12) {
    cb += sst_data[pos] != 0;
    pos++;
  }

  // BETTER: make cf dependent on expected relation between index_pos, index_size, sst_size
  CF icf = 1; 
  // must fall within file
  if (index_pos > sst_size - kFooterSize ||
      index_pos + index_size > sst_size - kFooterSize) {
    icf = 0;
  }
  footer.index = BlockHandle(index_pos, index_size, icf);

  //BETTER: as above
  CF mcf = 1; 
  // must fall within file
  if (metaindex_pos > sst_size - kFooterSize ||
      metaindex_pos + metaindex_size > sst_size - kFooterSize) {
    mcf = 0;
  }
  footer.metaindex = BlockHandle(metaindex_pos, metaindex_size, mcf);

  //BETTER more refined calc?
  CF cf = 1;
  if (cb > 12) cf = 0;
  if (cb > 8) cf = 0.1;
  if (cb > 4) cf = 0.2;
  if (cb > 0) cf = 0.3;
  //BETTER min seems too strict. weighted average?
  cf = std::min<CF>({cf, icf, mcf});
  footer.supp = BlockHandle(footer_start, sst_size - footer_start, cf);
  dout(20) << __func__ << std::hex << " footer: " << footer.supp << std::dec << dendl;
  return cf;
}




int SstCheck::examine()
{
  int r;
  bool b;
  CF cf;
  
  // attempt to read footer
  CF footer_cf = read_footer();
  
  //SstCheck::Index data_index;

  bool index_crc_ok = check_crc(footer.index.pos, footer.index.len);
  bool metaindex_crc_ok = check_crc(footer.metaindex.pos, footer.metaindex.len);

  // footer has no crc. its quality needs corroboration
  CF final_footer_cf = footer_cf * 0.5 + (index_crc_ok ? 0.25 : 0) + (metaindex_crc_ok ? 0.25 : 0);
  mark(BlockHandle(footer.supp.pos, footer.supp.len, final_footer_cf));
  
  mark(BlockHandle(footer.index.pos, footer.index.len + 4, /* crc length */
		   index_crc_ok ? 1 : 0));

  // read index of meta blocks
  cf = read_index_array(footer.metaindex.pos, footer.metaindex.pos + footer.metaindex.len,
			false, meta_index);
  
  CF meta_index_keys_cf =
    read_KB_values(footer.metaindex.pos, footer.metaindex.pos + footer.metaindex.len,
		   BlockMetaIndex, meta_index, meta_blocks_keys);
  mark(meta_index.supp);
  for (auto& k : meta_blocks_keys) {
    mark(k.supp);
  }
  //mark(meta_index.supp);
  dout(0) << __func__ << " meta_index_keys_cf=" << meta_index_keys_cf << dendl;

  // we do not check content of meta blocks, just od crc
  for (auto& mbl: meta_blocks_keys) {
    dout(10) << __func__ << " meta_block=" << mbl.name << dendl;
    bool crc_ok = check_crc(mbl.value.pos, mbl.value.len);
    dout(10) << __func__ << " crc_ok=" << crc_ok << dendl;
    mark(BlockHandle(mbl.value.pos, mbl.value.len, crc_ok ? 1.0 : 0.0));
  }

  // read index of data blocks
  cf = read_index_array(footer.index.pos, footer.index.pos + footer.index.len,
			false, data_index);
  mark(data_index.supp);
  CF data_index_keys_cf =
    read_KB_values(footer.index.pos, footer.index.pos + footer.index.len,
		   BlockDataIndex, data_index, data_blocks_keys);
  for (auto& dbl: data_blocks_keys) {
    dout(10) << __func__ << " data_block first_key=" << dbl.name
	     << " " << dbl.value << dendl;
    bool crc_ok = check_crc(dbl.value.pos, dbl.value.len);
    dout(10) << __func__ << " crc_ok=" << crc_ok << dendl;
  }
  for (auto& dbl: data_blocks_keys) {
    Index db_index;
    cf = read_index_array(dbl.value.pos,
			  dbl.value.pos + dbl.value.len,
			  true,
			  db_index);
    mark(db_index.supp);
    std::vector<KV> db_keys;
    CF db_keys_cf =
      read_KV_values(dbl.value.pos,
		     dbl.value.pos + dbl.value.len,
		     db_index,
		     db_keys);

    for (auto& k : db_keys) {
      mark(k.supp);
    }
  }

  // footer has metaindex and index

  // metaindex has properties:
  // - fullfilter.rocksdb.BuiltinBloomFilter (H)
  // - rocksdb.properties (H)
  // - others, but each should denote block (H)
  return 0;
}
