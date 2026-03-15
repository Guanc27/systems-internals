#include "allocator.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../p3malloc/smalloc.h"

typedef struct AllocHeader {
    size_t size;
} AllocHeader;

static AllocatorStats g_stats;
static int g_initialized;

static int should_fail_injected(void) {
    double draw;

    if (!g_stats.fault_mode_on || g_stats.fault_rate <= 0.0) {
        return 0;
    }
    draw = (double)rand() / (double)RAND_MAX;
    return draw < g_stats.fault_rate;
}

int allocator_init_from_env(void) {
    const char *backend;
    const char *fault_rate;

    if (g_initialized) {
        return 0;
    }

    memset(&g_stats, 0, sizeof(g_stats));
    srand((unsigned int)time(NULL));

    backend = getenv("MYSHELL_ALLOCATOR");
    if (backend != NULL && strcmp(backend, "smalloc") == 0) {
        if (my_init(4 * 1024 * 1024) == 0) {
            g_stats.using_smalloc = 1;
        }
    }

    if (getenv("MYSHELL_MEMFAULT") != NULL) {
        g_stats.fault_mode_on = 1;
    }
    fault_rate = getenv("MYSHELL_MEMFAULT_RATE");
    if (fault_rate != NULL) {
        g_stats.fault_rate = atof(fault_rate);
    }

    g_initialized = 1;
    return 0;
}

void *allocator_alloc(size_t size) {
    AllocHeader *h;
    size_t total;

    if (!g_initialized) {
        allocator_init_from_env();
    }

    g_stats.alloc_calls++;
    g_stats.bytes_requested += (unsigned long)size;

    if (should_fail_injected()) {
        g_stats.failed_allocs++;
        g_stats.injected_failures++;
        return NULL;
    }

    total = sizeof(AllocHeader) + size;

    if (g_stats.using_smalloc) {
        Malloc_Status st;
        h = (AllocHeader *)smalloc((int)total, &st);
        if (h == NULL || st.success != 1) {
            g_stats.failed_allocs++;
            return NULL;
        }
    } else {
        h = (AllocHeader *)malloc(total);
        if (h == NULL) {
            g_stats.failed_allocs++;
            return NULL;
        }
    }

    h->size = size;
    return (void *)(h + 1);
}

void allocator_free(void *ptr) {
    AllocHeader *h;

    if (ptr == NULL) {
        return;
    }
    g_stats.free_calls++;

    h = ((AllocHeader *)ptr) - 1;
    if (g_stats.using_smalloc) {
        sfree(h);
    } else {
        free(h);
    }
}

void allocator_set_fault_mode(int on) {
    g_stats.fault_mode_on = on ? 1 : 0;
}

int allocator_set_fault_rate(double rate) {
    if (rate < 0.0 || rate > 1.0) {
        return -1;
    }
    g_stats.fault_rate = rate;
    return 0;
}

const AllocatorStats *allocator_get_stats(void) {
    return &g_stats;
}
