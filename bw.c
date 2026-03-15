#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
#include <malloc.h>
#define aligned_alloc_custom(alignment, size) _aligned_malloc((size), (alignment))
#define aligned_free_custom(ptr) _aligned_free((ptr))
#else
#define aligned_alloc_custom(alignment, size) aligned_alloc((alignment), (size))
#define aligned_free_custom(ptr) free((ptr))
#endif

const size_t MIN_KB = 4;               // 4 KB
const size_t MAX_KB = 512 * 1024;      // 512 MB
const size_t Z2_KB = 32 * 1024;        // 32 MB
const size_t Z1_KB = Z2_KB / 32;       // 1 MB

static inline uint64_t rdtsc() {
#if defined(__x86_64__) || defined(_M_X64)
    unsigned hi, lo;
    __asm__ __volatile__ ("lfence\nrdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
#endif
}

size_t next_size(size_t size) {
    if (size < Z1_KB)
        return size + MIN_KB;
    if (size < Z2_KB)
        return size + Z1_KB;
    return size + Z2_KB;
}

int main(void) {
    FILE *data = fopen("data.csv", "w");
    if (!data) {
        perror("fopen");
        return 1;
    }

    const int reps = 5;  // measurement repetitions per point

    fprintf(data, "#size_kb");
    for (size_t stride = 64; stride <= 8192; stride *= 2) {
        fprintf(data, ",s%zu", stride);
    }
    fprintf(data, "\n");

    for (size_t sz_kb = MIN_KB; sz_kb <= MAX_KB; sz_kb = next_size(sz_kb)) {
        fprintf(data, "%zu", sz_kb);

        size_t sz = sz_kb * 1024ull;

        // For aligned_alloc on non-Windows, size must be a multiple of alignment.
        // Here sz is always a multiple of 1024, so also of 64.
        uint8_t *arr = (uint8_t *)aligned_alloc_custom(64, sz);
        if (!arr) {
            perror("alloc");
            fclose(data);
            return 1;
        }

        // Initialize: touch one byte per cache line / page progression
        for (size_t i = 0; i < sz; i += 64) {
            arr[i] = (uint8_t)i;
        }

        for (size_t stride = 64; stride <= 8192; stride *= 2) {
            size_t steps = sz / stride;

            if (steps == 0) {
                fprintf(data, ",NaN");
                continue;
            }

            volatile uint8_t sink = 0;

            // Warmup
            for (size_t i = 0, idx = 0; i < steps; ++i) {
                sink ^= arr[idx];
                idx += stride;
                if (idx >= sz) {
                    idx -= sz;
                }
            }

            double best_ns = 1e99;

            for (int r = 0; r < reps; ++r) {
                uint64_t t0 = rdtsc();

                for (size_t i = 0, idx = 0; i < steps; ++i) {
                    sink ^= arr[idx];
                    idx += stride;
                    if (idx >= sz) {
                        idx -= sz;
                    }
                }

                uint64_t t1 = rdtsc();

#if defined(__x86_64__) || defined(_M_X64)
                // Approximation: assume CPU around 4.8 GHz
                // 1 cycle ≈ 1/3 ns at 4.8 GHz
                double ns = (double)(t1 - t0) / 4.8;
#else
                double ns = (double)(t1 - t0);
#endif

                double ns_per_access = ns / (double)steps;
                if (ns_per_access < best_ns) {
                    best_ns = ns_per_access;
                }
            }

            if (sink == 255) {
                fprintf(stderr, "x");
            }

            fprintf(data, ",%.3f", best_ns);
        }

        aligned_free_custom(arr);
        fprintf(data, "\n");
    }

    fclose(data);

    system("gnuplot plot.gnuplot");
    printf("CSV and PDF files generated.\n");

    return 0;
}