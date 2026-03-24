#ifndef TRACE_H
#define TRACE_H

int trace_init_from_env(void);
void trace_log_command(const char *cmd, int status, const char *detail, const char *result, long elapsed_ms);
void trace_close(void);

#endif
