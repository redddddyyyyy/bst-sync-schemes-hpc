#include "test_helpers.h"
#include "bst_handover.h"

#include <pthread.h>
#include <stdint.h>
#include <set>
#include <vector>
#include <mutex>

namespace {

constexpr int N_THREADS    = 8;
constexpr int OPS_PER_THR  = 50000;
constexpr int KEYSPACE     = 2000;

struct ThreadArg {
    bst_handover_t *tree;
    std::vector<int> *inserted_keys;
    std::mutex *inserted_keys_mu;
    uint64_t seed;
};

// xorshift64* — local, deterministic.
static uint64_t xs64(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static void *worker(void *arg) {
    ThreadArg *a = static_cast<ThreadArg *>(arg);
    uint64_t state = a->seed ? a->seed : 0x9E3779B97F4A7C15ULL;
    std::vector<int> local_inserts;
    local_inserts.reserve(OPS_PER_THR / 2);

    for (int i = 0; i < OPS_PER_THR; ++i) {
        int key = static_cast<int>(xs64(&state) % KEYSPACE);
        // 50/50 insert vs search.
        if (xs64(&state) & 1ULL) {
            bst_handover_insert(a->tree, key);
            local_inserts.push_back(key);
        } else {
            bst_handover_search(a->tree, key);  // result ignored; just exercise the lock path
        }
    }

    std::lock_guard<std::mutex> g(*a->inserted_keys_mu);
    a->inserted_keys->insert(a->inserted_keys->end(),
                             local_inserts.begin(), local_inserts.end());
    return nullptr;
}

}  // namespace

int main(void) {
    bst_handover_t *t = bst_handover_create();
    ASSERT(t != NULL, "bst_handover_create returned NULL");

    std::vector<int> all_inserted;
    std::mutex all_inserted_mu;
    all_inserted.reserve(static_cast<size_t>(N_THREADS) * OPS_PER_THR / 2);

    pthread_t threads[N_THREADS];
    ThreadArg args[N_THREADS];
    for (int ti = 0; ti < N_THREADS; ++ti) {
        args[ti].tree              = t;
        args[ti].inserted_keys     = &all_inserted;
        args[ti].inserted_keys_mu  = &all_inserted_mu;
        args[ti].seed              = 0xC0FFEEULL + static_cast<uint64_t>(ti);
        if (pthread_create(&threads[ti], nullptr, worker, &args[ti]) != 0) {
            ASSERT(0, "pthread_create failed");
        }
    }
    for (int ti = 0; ti < N_THREADS; ++ti) {
        pthread_join(threads[ti], nullptr);
    }

    // Build the shadow: every distinct key inserted by any thread should be findable.
    std::set<int> shadow(all_inserted.begin(), all_inserted.end());

    long missing = 0;
    for (int k : shadow) {
        if (!bst_handover_search(t, k)) missing++;
    }
    ASSERT_EQ_LONG(missing, 0, "every inserted key must be findable post-join");

    // Conversely: every key NOT in the shadow should NOT be findable (in [0, KEYSPACE)).
    long spurious = 0;
    for (int k = 0; k < KEYSPACE; ++k) {
        if (shadow.find(k) == shadow.end() && bst_handover_search(t, k)) {
            spurious++;
        }
    }
    ASSERT_EQ_LONG(spurious, 0, "no key may be findable that no thread inserted");

    bst_handover_destroy(t);
    printf("OK test_handover_stress (n_threads=%d, ops=%d, keyspace=%d, distinct=%zu)\n",
           N_THREADS, OPS_PER_THR, KEYSPACE, shadow.size());
    return 0;
}
