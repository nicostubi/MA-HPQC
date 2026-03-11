#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

const size_t MIN_KB = 4;         // 4 KB
const size_t MAX_KB = 512 * 1024;       // 512 MB (adjust if RAM is small)
const size_t Z2_KB = MAX_KB/32;
const size_t Z1_KB = Z2_KB/32;


static inline uint64_t rdtsc() {
#if defined(__x86_64__)
    unsigned hi, lo;
    __asm__ __volatile__ ("lfence\nrdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    // Fallback (less precise): nanoseconds
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
#endif
}

size_t next_size(size_t size)
{
    if (size < Z1_KB)
        return size + MIN_KB;
    if (size < Z2_KB)
        return size + Z1_KB;
    return size + Z2_KB;
}

int main() {
    FILE *data = fopen("data", "w");
    const int reps = 5;              // measurement repetitions per point

    fprintf(data, "#size");
    for (size_t stride = 64; stride <= 8192; stride *= 2)
    	fprintf(data, ", s%zu", stride);
    fprintf(data, "\n");
    for (size_t sz_kb = MIN_KB; sz_kb <= MAX_KB; sz_kb = next_size(sz_kb)) {
    	fprintf(data, "%zu", sz_kb);
        size_t sz = sz_kb * 1024ull;
        uint8_t *arr = aligned_alloc(64, sz);
        if (!arr) { perror("alloc"); return 1; }

        // Initialize to touch all pages
        for (size_t i = 0; i < sz; i += 64) arr[i] = (uint8_t)i;

        for (size_t stride = 64; stride <= 8192; stride *= 2) {
            // Make index sequence wrap around the array
            size_t steps = sz / stride;
            if (steps == 0) continue;

            // Warmup
            volatile uint8_t sink = 0;
            for (size_t i = 0, idx = 0; i < steps; ++i) {
                sink ^= arr[idx];
                idx += stride;
                if (idx >= sz) idx -= sz;
            }

            // Measure multiple reps and take best (robust to noise)
            double best_ns = 1e99;
            for (int r = 0; r < reps; ++r) {
                uint64_t t0 = rdtsc();
                for (size_t i = 0, idx = 0; i < steps; ++i) {
                    sink ^= arr[idx];
                    idx += stride;
                    if (idx >= sz) idx -= sz;
                }
                uint64_t t1 = rdtsc();

#if defined(__x86_64__)
                // Convert cycles to ns using rough frequency (students should calibrate)
                // Better: read /proc/cpuinfo or use perf to get cycles + scale outside.
                // For a first pass, assume ~3.0 GHz:
                double ns = (double)(t1 - t0) / 3.0;
#else
                double ns = (double)(t1 - t0);
#endif
                double ns_per_access = ns / (double)steps;
                if (ns_per_access < best_ns) best_ns = ns_per_access;
            }
            // Prevent optimizer removing sink
            if (sink == 255) fprintf(stderr, "x");

            fprintf(data, ", %.3f", best_ns);
        }
        free(arr);
        fprintf(data, "\n");
    }
    fclose(data);
    system("gnuplot plot.gnuplot");
    printf("PDF files generated.\n");
    return 0;
}
