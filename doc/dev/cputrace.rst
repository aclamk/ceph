
========
CpuTrace
========

CpuTrace is a tool to measure hardware cost of program execution.
Its intended user is a developer; either when he writes new code and
cannot decide which algorithm is better, or when he has an idea for
a possible performance optimization, but wants to make sure.
CpuTrace measures cpu instructions, clock cycles, branch mispredictions,
cache misses and thread reschedules.

Raw counter mode
----------------

CpuTrace is using linux ``perf_even_open`` syscall. You can use the tool
as a simple helper to let get access to hardware perf counters.

.. code-block:: cpp
  // I am profiling my code and want to know
  // how many clock cycles and how many thread switches it takes
  HW_ctx hw = HW_ctx_empty;
  HW_init(&hw, HW_PROFILE_SWI|HW_PROFILE_CYC);
  sample_t start, end;
  HW_read(&hw, &start);
  // my code starts
  // .....
  // my code ends
  HW_read(&hw, &end);
  // task_switches = end.swi - start.swi;
  // clock_cycles  = end.cyc - start.cyc;
  HW_clean(&hw);

By inspecting ``task_switches`` and ``clock_cycles`` developer can learn that
real clock execution time of 10ms has only 1M clock cycles, but had 2 task switches.


Aggregating samples
-------------------

Single readout of execution time is usually not enough. We need more samples
to get more realistic measurement of actual execution cost.

.. code-block:: cpp
  // a variable to hold my measurement
  static measurement_t my_code_time;
  sample_t start, end, elapsed;
  // hw initialized somewhere else
  HW_read(&hw, &start);
  // my code starts
  // .....
  // my code ends
  HW_read(&hw, &end);
  elapsed = end - start;
  // add new sample to the whole measurement
  my_code_time.sample(elapsed);

The ``measurement_t`` type is aggregating samples and counting how many measurement
were done.

TODO: write about measurement_t ability to print results in various formats

TODO: think of better name and that measurement_t might be a zoo of differnent 
      data processing helpers: average, min-max, split results between 0 and non-0 task switches ....


RAII samples
------------

TODO: write that usually it is most convenient to use RAII to collect samples
      explain that ``hw`` has to be initialized earlier

.. code-block:: cpp
  // a variable to hold my measurement
  static measurement_t my_code_time;
  {
    HW_guard(&hw, &my_code_time);
    // my code starts
    // .....
    // my code ends
  }


Measurement groups
------------------

TODO: write about infrastructure that allows one to simplify hw measurements
      organization into groups

TODO: write that groups must be earlier initialized (HW_init)

TODO: invent actual API how to do it seamless

.. code-block:: cpp
  {
    constexpr uint8_t group_id = 2;
    HW_named_guard("my_code_time", group_id); //if not provided, it is 0
    // my code starts
    // .....
    // my code ends
  }








Admin socket integration
------------------------

TODO: write that there is a usually most useful to have control over measurement groups
      by admin socket commands

TODO: explain how to use commands and give examples for start/stop




UNORDERED DOC BELOW

.. code-block:: cpp
  ... myfunction()
  {
    HW_profile profiling_my_function("my_code");
    // my code starts
    // .....
    // my code ends
  }

level 

.. code-block:: cpp

  HW_ctx my_counters = HW_ctx_empty;

  enum cputrace_result_type {
    CPUTRACE_RESULT_SWI = 0,
    CPUTRACE_RESULT_CYC,
    CPUTRACE_RESULT_CMISS,
    CPUTRACE_RESULT_BMISS,
    CPUTRACE_RESULT_INS,
    CPUTRACE_RESULT_CALL_COUNT,
    CPUTRACE_RESULT_COUNT
  };
  enum cputrace_flags {
    HW_PROFILE_SWI   = (1ULL << CPUTRACE_RESULT_SWI),
    HW_PROFILE_CYC   = (1ULL << CPUTRACE_RESULT_CYC),
    HW_PROFILE_CMISS = (1ULL << CPUTRACE_RESULT_CMISS),
    HW_PROFILE_BMISS = (1ULL << CPUTRACE_RESULT_BMISS),
    HW_PROFILE_INS   = (1ULL << CPUTRACE_RESULT_INS),
  };

  extern HW_ctx HW_ctx_empty;
  // initializes hardware resources for capturing specific performance counters
  bool HW_init(HW_ctx* ctx, cputrace_flags counters_to_measure);
  // closes hardware resources
  void HW_clean(HW_ctx* ctx)
  // read current values of tracked counters
  void HW_read(HW_ctx* ctx, sample_t reads);

  struct sample_t {
    uint64_t swi;   //context switches
    uint64_t cyc;   //clock cycles
    uint64_t cmiss; //cache misses
    uint64_t bmiss; //branch misses
    uint64_t ins;   //instructions
  };





DOCUMENT ENDS HERE, rest to delete


When a structure is sent over the network or written to disk, it is
encoded into a string of bytes. Usually (but not always -- multiple
serialization facilities coexist in Ceph) serializable structures
have ``encode`` and ``decode`` methods that write and read from
``bufferlist`` objects representing byte strings.

Terminology
-----------
It is best to think not in the domain of daemons and clients but
encoders and decoders. An encoder serializes a structure into a bufferlist
while a decoder does the opposite.

Encoders and decoders can be referred collectively as dencoders.

Dencoders (both encoders and docoders) live within daemons and clients.
For instance, when an RBD client issues an IO operation, it prepares
an instance of the ``MOSDOp`` structure and encodes it into a bufferlist
that is put on the wire.
An OSD reads these bytes and decodes them back into an ``MOSDOp`` instance.
Here encoder was used by the client while decoder by the OSD. However,
these roles can swing -- just imagine handling of the response: OSD encodes
the ``MOSDOpReply`` while RBD clients decode.

Encoder and decoder operate accordingly to a format which is defined
by a programmer by implementing the ``encode`` and ``decode`` methods.

Principles for format change
----------------------------
It is not unusual for the format of serialization to change. This
process requires careful attention both during development
and review.

The general rule is that a decoder must understand what has been encoded by an
encoder. Most difficulties arise during the process of ensuring the continuity
of compatibility of old decoders with new encoders, and ensuring the continuity
of compatibility of new decoders with old decoders. One should assume -- if not
otherwise specified -- that any mix of old and new is possible in a cluster.
There are two primary concerns:

1. **Upgrades.** Although there are recommendations related to the order of
   entity types (mons/OSDs/clients), it is not mandatory and no assumption
   should be made.
2. **Huge variability of client versions.** It has always been the case that
   kernel upgrades (and thus kernel clients) are decoupled from Ceph upgrades.
   Containerization brings variability even to ``librbd`` -- now user space
   libraries live in the container itself:

There are a few rules limiting the degree of interoperability between
dencoders:

* ``n-2`` for dencoding between daemons,
* ``n-3`` hard requirement for client scenarios,
* ``n-3..`` soft requirement for client scenarios. Ideally every client should
  be able to talk to any version of daemons.

As the underlying reasons are the same, the rules that dencoders
follow are nearly the same as the rules for deprecations of our features
bits. See the ``Notes on deprecation`` in ``src/include/ceph_features.h``.

Frameworks
----------
Currently multiple genres of dencoding helpers co-exist.

* encoding.h (the most proliferated one),
* denc.h (performance optimized, seen mostly in ``BlueStore``),
* the `Message` hierarchy.

Although details vary, the interoperability rules stay the same.

Adding a field to a structure
-----------------------------

You can see examples of this all over the Ceph code, but here's an
example:

.. code-block:: cpp

    class AcmeClass
    {
        int member1;
        std::string member2;

        void encode(bufferlist &bl)
        {
            ENCODE_START(1, 1, bl);
            ::encode(member1, bl);
            ::encode(member2, bl);
            ENCODE_FINISH(bl);
        }

        void decode(bufferlist::iterator &bl)
        {
            DECODE_START(1, bl);
            ::decode(member1, bl);
            ::decode(member2, bl);
            DECODE_FINISH(bl);
        }
    };

The ``ENCODE_START`` macro writes a header that specifies a *version* and
a *compat_version* (both initially 1).  The message version is incremented
whenever a change is made to the encoding.  The compat_version is incremented
only if the change will break existing decoders -- decoders are tolerant
of trailing bytes, so changes that add fields at the end of the structure
do not require incrementing compat_version.

The ``DECODE_START`` macro takes an argument specifying the most recent
message version that the code can handle.  This is compared with the
compat_version encoded in the message, and if the message is too new then
an exception will be thrown.  Because changes to compat_version are rare,
this isn't usually something to worry about when adding fields.

In practice, changes to encoding usually involve simply adding the desired fields
at the end of the ``encode`` and ``decode`` functions, and incrementing
the versions in ``ENCODE_START`` and ``DECODE_START``.  For example, here's how
to add a third field to ``AcmeClass``:

.. code-block:: cpp

    class AcmeClass
    {
        int member1;
        std::string member2;
        std::vector<std::string> member3;

        void encode(bufferlist &bl)
        {
            ENCODE_START(2, 1, bl);
            ::encode(member1, bl);
            ::encode(member2, bl);
            ::encode(member3, bl);
            ENCODE_FINISH(bl);
        }

        void decode(bufferlist::iterator &bl)
        {
            DECODE_START(2, bl);
            ::decode(member1, bl);
            ::decode(member2, bl);
            if (struct_v >= 2) {
                ::decode(member3, bl);
            }
            DECODE_FINISH(bl);
        }
    };

Note that the compat_version did not change because the encoded message
will still be decodable by versions of the code that only understand
version 1 -- they will just ignore the trailing bytes where we encode ``member3``.

In the ``decode`` function, decoding the new field is conditional: this is
because we might still be passed older-versioned messages that do not
have the field.  The ``struct_v`` variable is a local set by the ``DECODE_START``
macro.

# Into the weeeds

The append-extendability of our dencoders is a result of the forward
compatibility that the ``ENCODE_START`` and ``DECODE_FINISH`` macros bring.

They are implementing extensibility facilities. An encoder, when filling
the bufferlist, prepends three fields: version of the current format,
minimal version of a decoder compatible with it and the total size of
all encoded fields.

.. code-block:: cpp

        /**
         * start encoding block
         *
         * @param v current (code) version of the encoding
         * @param compat oldest code version that can decode it
         * @param bl bufferlist to encode to
         *
         */
        #define ENCODE_START(v, compat, bl)                             \
          __u8 struct_v = v;                                            \
          __u8 struct_compat = compat;                                  \
          ceph_le32 struct_len;                                         \
          auto filler = (bl).append_hole(sizeof(struct_v) +             \
            sizeof(struct_compat) + sizeof(struct_len));                \
          const auto starting_bl_len = (bl).length();                   \
          using ::ceph::encode;                                         \
          do {

The ``struct_len`` field allows the decoder to eat all the bytes that were
left undecoded in the user-provided ``decode`` implementation.
Analogically, decoders tracks how much input has been decoded in the
user-provided ``decode`` methods.

.. code-block:: cpp

        #define DECODE_START(bl)		                        \
          unsigned struct_end = 0;					\
          __u32 struct_len;						\
          decode(struct_len, bl);					\
          ...                                                           \
          struct_end = bl.get_off() + struct_len;			\
          }								\
          do {


Decoder uses this information to discard the extra bytes it does not
understand. Advancing bufferlist is critical as dencoders tend to be nested;
just leaving it intact would work only for the very last ``deocde`` call
in a nested structure.

.. code-block:: cpp

        #define DECODE_FINISH(bl)					\
          } while (false);						\
          if (struct_end) {						\
            ...                                                         \
            if (bl.get_off() < struct_end)				\
              bl += struct_end - bl.get_off();				\
          }


This entire, cooperative mechanism allows encoder (its further revisions)
to generate more byte stream (due to e.g. adding a new field at the end)
and not worry that the residue will crash older decoder revisions.
