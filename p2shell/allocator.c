#include "allocator.h"

#include <stddef.h>

#include "../p3malloc/smalloc.h"

extern void *malloc(size_t);
extern void free(void *);
extern char *getenv(const char *);

typedef struct AllocHeader {
    size_t size;
} AllocHeader;

static AllocatorStats g_stats;
static int g_initialized;
static unsigned long g_rng_state = 0x9E3779B9UL;

static int streq(const char *a, const char *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static double parse_rate_01(const char *s, int *ok) {
    int seen_dot = 0;
    double whole = 0.0;
    double frac = 0.0;
    double base = 1.0;

    *ok = 0;
    if (s == NULL || *s == '\0') {
        return 0.0;
    }
    while (*s != '\0') {
        if (*s == '.') {
            if (seen_dot) {
                return 0.0;
            }
            seen_dot = 1;
            s++;
            continue;
        }
        if (*s < '0' || *s > '9') {
            return 0.0;
        }
        if (!seen_dot) {
            whole = whole * 10.0 + (double)(*s - '0');
        } else {
            base *= 10.0;
            frac += (double)(*s - '0') / base;
        }
        s++;
    }
    *ok = 1;
    return whole + frac;
}

static double next_rand_unit(void) {
    g_rng_state = g_rng_state * 1664525UL + 1013904223UL;
    return (double)(g_rng_state & 0xFFFFFFUL) / (double)0x1000000UL;
}

static int should_fail_injected(void) {
    double draw;

    if (!g_stats.fault_mode_on || g_stats.fault_rate <= 0.0) {
        return 0;
    }
    draw = next_rand_unit();
    return draw < g_stats.fault_rate;
}

int allocator_init_from_env(void) {
    const char *backend;
    const char *fault_rate;
    int ok = 0;

    if (g_initialized) {
        return 0;
    }

    g_stats = (AllocatorStats){0};

    backend = getenv("MYSHELL_ALLOCATOR");
    if (backend != NULL && streq(backend, "smalloc")) {
        if (my_init(4 * 1024 * 1024) == 0) {
            g_stats.using_smalloc = 1;
        }
    }

    if (getenv("MYSHELL_MEMFAULT") != NULL) {
        g_stats.fault_mode_on = 1;
    }
    fault_rate = getenv("MYSHELL_MEMFAULT_RATE");
    if (fault_rate != NULL) {
        g_stats.fault_rate = parse_rate_01(fault_rate, &ok);
        if (!ok) {
            g_stats.fault_rate = 0.0;
        }
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
