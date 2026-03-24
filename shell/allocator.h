#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

typedef struct AllocatorStats {
    unsigned long alloc_calls;
    unsigned long free_calls;
    unsigned long failed_allocs;
    unsigned long injected_failures;
    unsigned long bytes_requested;
    int using_smalloc;
    int fault_mode_on;
    double fault_rate;
} AllocatorStats;

int allocator_init_from_env(void);
void *allocator_alloc(size_t size);
void allocator_free(void *ptr);
void allocator_set_fault_mode(int on);
int allocator_set_fault_rate(double rate);
const AllocatorStats *allocator_get_stats(void);

#endif
