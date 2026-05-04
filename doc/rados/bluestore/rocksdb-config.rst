============================
 RocksDB Config Reference
============================

.. note:: There are 2 places in ceph that use RocksDB: MON and OSD.
   This document focuses on OSD's RocksDB.

.. index:: bluestore; rocksdb caching

RocksDB caching
===============

RocksDB caching it based on preserving parts of .sst files in `block cache`
https://github.com/facebook/rocksdb/wiki/Block-Cache.
Ceph implements its own flavour of block cache 
https://github.com/ceph/ceph/tree/main/src/kv/rocksdb_cache.
This custom implementation brings rocksdb block cache, bluestore metadata cache
and bluestore data cache together to compete for available memory.

Cache sharding
--------------

As the defult RockDB block cache, ceph block cache is sharded.
Sharding is controlled by configuration. The purpose of sharding is to
steamline multi threaded access.

``rocksdb_cache_shard_bits``

:Description: Defines how many bits of key hash define shard.
              4 bit -> 16 shards
:Type: Unsigned 32-bit Integer
:Default: 4

Perf counters
-------------

Ceph RocksDB cache operations are tracked by performance counters.
In default configuration BlueStore creates 2 block caches: one for onodes ``O``,
and one for everything else ``default``.
The sections created in performance counters are named ``rocksdb-cache-O`` and
``rocksdb-cache-default``.

.. code-block:: 
  :caption: cache stats for onode metadata

  "rocksdb-cache-O": {
    "capacity": 134217728,
    "usage": 134182832,
    "pinned": 0,
    "elems": 24502,
    "inserts": 25806978,
    "lookups": 150436987,
    "hits": 124629911,
    "misses": 25807076
  }

Values ``capacity``, ``usage``, ``pinned`` and ``elems`` reflect current state of the cache.
Values ``inserts``, ``lookups``, ``hits`` and ``misses`` are increased on event.

.. index:: rocksdb; perf counters

Admin commands
--------------

Performance counters show a brief summation, but in reality each cache shard has its own stats.
Admin socket command allows to inspect shards details.

.. prompt:: bash #

  ceph tell osd.0 rocksdb show cache O

.. code-block::

    shard  capacity     usage   pinned   elems  inserts  lookups     hits   misses
        0  13631488  11076400        0    2099   136987   822679   685923   136756
        1  13631488  11549712        0    2043   133359   571500   438383   133117
        2  13631488  11060608        0    2232   135076   908468   773313   135155
        3  13631488  11166896        0    2269   134006   427070   293147   133923
        4  13631488  11117984        0    2297   133367   700242   567318   132924
        5  13631488  11306672        0    2155   137501  1130135   991810   138325
        6  13631488  11506512        0    2353   134515   662792   528514   134278
        7  13631488  11093856        0    2316   135348   718971   583421   135550
        8  13631488  11660624        0    2424   137363  1092043   954248   137795
        9  13631488  10962000        0    2561   131982   431702   300467   131235
       10  13631488  11379392        0    1916   134543   477118   342854   134264
       11  13631488  11294272        0    2555   134508   512393   378337   134056
       12  13631488  11277136        0    2079   137312  1131571   993692   137879
       13  13631488  10887776        0    2543   134001   567073   432903   134170
       14  13631488  10986528        0    2394   133288   584452   451018   133434
       15  13631488  11954464        0    2456   134615   708285   573374   134911

Resetting clearable counters to 0.

.. prompt:: bash #

   ceph tell osd.0 rocksdb reset cache O

.. index:: bluestore; rocksdb; admin commands

Optimum shard count
-------------------

In most cases 16 shards as defined by ``rocksdb_cache_shard_bits=4`` is a good choice.
Large OSDs can easily accomodate millions of objects and thus having milions of keys
to encode Onode metadata. While number of keys is not directly a problem,
it causes RocksDB to create very large index blocks.
During leveled compation RocksDB merges 2 levels. It means that 2 index  blocks are
needed at the same time.
When 1) index block size > 0.5 shard size 2) both index blocks belong to same shard
cache begins to flicker; a need to access one index block causes eviction of the other block.
When such condition is active performance is severely degraded.

Flickering detection
********************

.. prompt:: bash #

  ceph tell osd.0 rocksdb show cache O

.. code-block::

    shard  capacity     usage   pinned   elems  inserts  lookups     hits   misses
      ...
        2  13631488  11060608        0    2232   135076   908468   773313   135155
        3  13631488   8166896        0       1   134006   427070   293147   133923
        4  13631488  11117984        0    2297   133367   700242   567318   132924
      ...

It is likely that shard 3 is doing constant eviction. To verify, observe the relation
between ``misses`` and ``hits``. Reseting values 0 helps.
Also, perf counter for ``bluefs.read_bytes`` will is rising very fast when rocksdb is
reading same index blocks over and over again.

Mitigation
**********

More like a workaround. Reduce ``rocksdb_cache_shard_bits``. This will affect baseline
rocksdb performance by a bit overall, as less shards means more opportunity for lock
contention.