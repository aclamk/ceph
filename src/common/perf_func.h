#include <atomic>
namespace ceph {
class Formatter;
}

class perf_entry {
public:
        const char* function = nullptr;
	std::atomic<uint64_t> count{0};
	std::atomic<uint64_t> total{0};
        perf_entry(const char* function) : function(function) {entries.emplace_back(this);};

        static std::vector<perf_entry*> entries;
        static void dump(ceph::Formatter* f);
};


extern thread_local uint32_t perf_depth;
class perf_measure {
  uint64_t time_now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
  }
public:
        perf_entry& pe;
        uint64_t start = 0;
        perf_measure(perf_entry& pe) :pe(pe) {
          if (perf_depth == 0) {
            start = time_now();
          };
          perf_depth++;
        }
        ~perf_measure() {
          perf_depth--;
          if (perf_depth == 0) {
            pe.total += time_now() - start;
          };
          pe.count++;
        }
};
#define TOKENCONCATX(x, y) x ## y
#define TOKENCONCAT(x, y) TOKENCONCATX(x, y)
#define PROFILE_THIS_FUNCTION() \
static perf_entry TOKENCONCAT(perf_entry__automatic,__LINE__)(__PRETTY_FUNCTION__); \
  perf_measure TOKENCONCAT(perf_measure__automatic,__LINE__)(TOKENCONCAT(perf_entry__automatic,__LINE__))

