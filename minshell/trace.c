#define _POSIX_C_SOURCE 200809L

#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *g_trace_file = NULL;

static void json_escape(const char *src, char *dst, size_t dst_len) {
    size_t i = 0;

    if (src == NULL || dst == NULL || dst_len == 0) {
        return;
    }

    while (*src != '\0' && i + 2 < dst_len) {
        if (*src == '"' || *src == '\\') {
            if (i + 3 >= dst_len) {
                break;
            }
            dst[i++] = '\\';
            dst[i++] = *src++;
            continue;
        }
        if (*src == '\n') {
            if (i + 3 >= dst_len) {
                break;
            }
            dst[i++] = '\\';
            dst[i++] = 'n';
            src++;
            continue;
        }
        dst[i++] = *src++;
    }
    dst[i] = '\0';
}

int trace_init_from_env(void) {
    const char *path = getenv("MYSHELL_TRACE");
    if (path == NULL || strlen(path) == 0) {
        return 0;
    }

    g_trace_file = fopen(path, "a");
    if (g_trace_file == NULL) {
        return -1;
    }
    return 0;
}

void trace_log_command(const char *cmd, int status, const char *detail, const char *result, long elapsed_ms) {
    struct timespec ts;
    char esc_cmd[1024];
    char esc_detail[512];
    char esc_result[512];

    if (g_trace_file == NULL) {
        return;
    }

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    }

    json_escape(cmd == NULL ? "" : cmd, esc_cmd, sizeof(esc_cmd));
    json_escape(detail == NULL ? "" : detail, esc_detail, sizeof(esc_detail));
    json_escape(result == NULL ? "" : result, esc_result, sizeof(esc_result));

    (void)fprintf(g_trace_file,
                  "{\"ts_sec\":%ld,\"ts_nsec\":%ld,\"cmd\":\"%s\",\"status\":%d,\"elapsed_ms\":%ld,"
                  "\"detail\":\"%s\",\"result\":\"%s\"}\n",
                  (long)ts.tv_sec, (long)ts.tv_nsec, esc_cmd, status, elapsed_ms, esc_detail, esc_result);
    (void)fflush(g_trace_file);
}

void trace_close(void) {
    if (g_trace_file != NULL) {
        fclose(g_trace_file);
        g_trace_file = NULL;
    }
}
