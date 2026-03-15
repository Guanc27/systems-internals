#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "allocator.h"
#include "sim_device.h"
#include "trace.h"

#define MAX_ARGS 64
#define MAX_PATH 1024
#define MAX_COMMANDS 64
#define MAX_LINE 514
#define MAX_RESULT 512

static char error_message[] = "An error has occurred\n";
static char g_last_result[MAX_RESULT];

static void my_print(const char *msg) {
    (void)write(STDOUT_FILENO, msg, strlen(msg));
}

static void print_error(void) {
    (void)write(STDOUT_FILENO, error_message, strlen(error_message));
}

static long monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void sleep_millis(int ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
}

static void set_last_result(const char *value) {
    if (value == NULL) {
        g_last_result[0] = '\0';
        return;
    }
    (void)snprintf(g_last_result, sizeof(g_last_result), "%s", value);
}

static void discard_line(FILE *input) {
    int c;
    while ((c = fgetc(input)) != '\n' && c != EOF) {
    }
}

static char *trim_whitespace(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    return str;
}

static int parse_args(char *cmd, char **argv) {
    int argc = 0;
    char *token = strtok(cmd, " \t");

    while (token != NULL && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    return argc;
}

static int parse_int(const char *text, int *out) {
    char *end;
    long v;
    if (text == NULL || *text == '\0') {
        return -1;
    }
    errno = 0;
    v = strtol(text, &end, 0);
    if (errno != 0 || *end != '\0' || v < -2147483647L || v > 2147483647L) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int parse_redirection(char *cmd, char **prog_part, char **outfile, int *advanced) {
    char *redir = strchr(cmd, '>');
    char *next;

    *advanced = 0;
    *prog_part = cmd;
    *outfile = NULL;

    if (redir == NULL) {
        return 0;
    }
    next = strchr(redir + 1, '>');
    if (next != NULL) {
        return -1;
    }

    if (*(redir + 1) == '+') {
        *advanced = 1;
        *redir = '\0';
        *outfile = trim_whitespace(redir + 2);
    } else {
        *redir = '\0';
        *outfile = trim_whitespace(redir + 1);
    }
    *prog_part = trim_whitespace(cmd);

    if ((*outfile) == NULL || (*outfile)[0] == '\0') {
        return -1;
    }
    if (strchr(*outfile, ' ') != NULL || strchr(*outfile, '\t') != NULL) {
        return -1;
    }
    if ((*prog_part)[0] == '\0') {
        return -1;
    }
    return 1;
}

static int run_external(char **argv) {
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        print_error();
        _exit(1);
    }

    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }
    return 1;
}

static int run_external_with_redirection(char **argv, const char *outfile, int advanced) {
    int out_fd;
    pid_t pid;
    int status;
    char old_data[32768];
    ssize_t old_sz = 0;

    if (!advanced && access(outfile, F_OK) == 0) {
        return -1;
    }

    if (advanced && access(outfile, F_OK) == 0) {
        int old_fd = open(outfile, O_RDONLY);
        if (old_fd >= 0) {
            old_sz = read(old_fd, old_data, sizeof(old_data));
            if (old_sz < 0) {
                old_sz = 0;
            }
            close(old_fd);
        }
    }

    out_fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (out_fd < 0) {
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        close(out_fd);
        return -1;
    }
    if (pid == 0) {
        if (dup2(out_fd, STDOUT_FILENO) < 0) {
            _exit(1);
        }
        close(out_fd);
        execvp(argv[0], argv);
        print_error();
        _exit(1);
    }

    close(out_fd);
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }

    if (advanced && old_sz > 0) {
        int append_fd = open(outfile, O_WRONLY | O_APPEND);
        if (append_fd >= 0) {
            (void)write(append_fd, old_data, (size_t)old_sz);
            close(append_fd);
        }
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }
    return 1;
}

static int run_protocol_builtin(char **argv, int argc) {
    int bus;
    int addr;
    int reg;
    int len;
    int i;
    unsigned char tx[64];
    unsigned char rx[64];
    unsigned short mdio_val;
    char result[256];

    if (strcmp(argv[0], "i2c_read") == 0) {
        if (argc != 5 || parse_int(argv[1], &bus) != 0 || parse_int(argv[2], &addr) != 0 ||
            parse_int(argv[3], &reg) != 0 || parse_int(argv[4], &len) != 0 || len <= 0 || len > 64) {
            return -1;
        }
        if (sim_i2c_read(bus, addr, reg, len, rx) != 0) {
            return -1;
        }
        result[0] = '\0';
        for (i = 0; i < len; i++) {
            char one[12];
            (void)snprintf(one, sizeof(one), "0x%02X%s", rx[i], (i + 1 == len) ? "" : " ");
            if (strlen(result) + strlen(one) + 1 < sizeof(result)) {
                strcat(result, one);
            }
        }
        strcat(result, "\n");
        my_print(result);
        set_last_result(result);
        sim_tick();
        return 0;
    }

    if (strcmp(argv[0], "i2c_write") == 0) {
        if (argc < 5 || parse_int(argv[1], &bus) != 0 || parse_int(argv[2], &addr) != 0 ||
            parse_int(argv[3], &reg) != 0 || argc - 4 > 64) {
            return -1;
        }
        len = argc - 4;
        for (i = 0; i < len; i++) {
            int byte_val;
            if (parse_int(argv[4 + i], &byte_val) != 0 || byte_val < 0 || byte_val > 255) {
                return -1;
            }
            tx[i] = (unsigned char)byte_val;
        }
        if (sim_i2c_write(bus, addr, reg, tx, len) != 0) {
            return -1;
        }
        my_print("OK\n");
        set_last_result("OK");
        sim_tick();
        return 0;
    }

    if (strcmp(argv[0], "spi_xfer") == 0) {
        if (argc < 4 || parse_int(argv[1], &bus) != 0 || parse_int(argv[2], &addr) != 0 || argc - 3 > 64) {
            return -1;
        }
        len = argc - 3;
        for (i = 0; i < len; i++) {
            int byte_val;
            if (parse_int(argv[3 + i], &byte_val) != 0 || byte_val < 0 || byte_val > 255) {
                return -1;
            }
            tx[i] = (unsigned char)byte_val;
        }
        if (sim_spi_xfer(bus, addr, tx, len, rx) != 0) {
            return -1;
        }
        result[0] = '\0';
        for (i = 0; i < len; i++) {
            char one[12];
            (void)snprintf(one, sizeof(one), "0x%02X%s", rx[i], (i + 1 == len) ? "" : " ");
            if (strlen(result) + strlen(one) + 1 < sizeof(result)) {
                strcat(result, one);
            }
        }
        strcat(result, "\n");
        my_print(result);
        set_last_result(result);
        sim_tick();
        return 0;
    }

    if (strcmp(argv[0], "mdio_read") == 0) {
        if (argc != 4 || parse_int(argv[1], &bus) != 0 || parse_int(argv[2], &addr) != 0 ||
            parse_int(argv[3], &reg) != 0) {
            return -1;
        }
        if (sim_mdio_read(bus, addr, reg, &mdio_val) != 0) {
            return -1;
        }
        (void)snprintf(result, sizeof(result), "0x%04X\n", mdio_val);
        my_print(result);
        set_last_result(result);
        sim_tick();
        return 0;
    }

    if (strcmp(argv[0], "uart_send") == 0) {
        if (argc < 3 || parse_int(argv[1], &bus) != 0) {
            return -1;
        }
        result[0] = '\0';
        for (i = 2; i < argc; i++) {
            if (strlen(result) + strlen(argv[i]) + 2 < sizeof(result)) {
                if (i > 2) {
                    strcat(result, " ");
                }
                strcat(result, argv[i]);
            }
        }
        if (sim_uart_send(bus, result) != 0) {
            return -1;
        }
        my_print("UART_OK\n");
        set_last_result(sim_last_uart());
        sim_tick();
        return 0;
    }

    if (strcmp(argv[0], "dump_regs") == 0) {
        int start = 0;
        int count = 16;
        char dump[1024];

        if (argc >= 2 && parse_int(argv[1], &start) != 0) {
            return -1;
        }
        if (argc >= 3 && parse_int(argv[2], &count) != 0) {
            return -1;
        }
        sim_dump_regs(start, count, dump, sizeof(dump));
        my_print(dump);
        my_print("\n");
        set_last_result(dump);
        return 0;
    }

    return 2;
}

static void print_memstat(void) {
    const AllocatorStats *stats = allocator_get_stats();
    char buf[256];
    (void)snprintf(buf, sizeof(buf),
                   "alloc_calls=%lu free_calls=%lu failed_allocs=%lu injected_failures=%lu "
                   "bytes_requested=%lu backend=%s fault_mode=%d fault_rate=%.3f\n",
                   stats->alloc_calls, stats->free_calls, stats->failed_allocs, stats->injected_failures,
                   stats->bytes_requested, stats->using_smalloc ? "smalloc" : "libc", stats->fault_mode_on,
                   stats->fault_rate);
    my_print(buf);
    set_last_result(buf);
}

static int execute_single(char *cmd);

static int run_control_builtin(char **argv, int argc, const char *raw_cmd) {
    int ms;
    int retries;
    int i;
    int status;
    char *subcmd;
    long start_ms;
    long elapsed;
    double rate;

    if (strcmp(argv[0], "sleep_ms") == 0) {
        if (argc != 2 || parse_int(argv[1], &ms) != 0 || ms < 0) {
            return -1;
        }
        sleep_millis(ms);
        set_last_result("SLEEP_DONE");
        return 0;
    }

    if (strcmp(argv[0], "assert_eq") == 0) {
        const char *lhs;
        const char *rhs;
        if (argc != 3) {
            return -1;
        }
        lhs = (strcmp(argv[1], "$LAST") == 0) ? g_last_result : argv[1];
        rhs = (strcmp(argv[2], "$LAST") == 0) ? g_last_result : argv[2];
        if (strcmp(lhs, rhs) != 0) {
            return 1;
        }
        my_print("ASSERT_OK\n");
        set_last_result("ASSERT_OK");
        return 0;
    }

    if (strcmp(argv[0], "retry") == 0) {
        if (argc < 3 || parse_int(argv[1], &retries) != 0 || retries < 1) {
            return -1;
        }
        subcmd = strstr(raw_cmd, argv[2]);
        if (subcmd == NULL) {
            return -1;
        }
        for (i = 0; i < retries; i++) {
            char *buf = allocator_alloc(strlen(subcmd) + 1);
            if (buf == NULL) {
                return -1;
            }
            strcpy(buf, subcmd);
            status = execute_single(buf);
            allocator_free(buf);
            if (status == 0) {
                return 0;
            }
            sim_tick();
        }
        return 1;
    }

    if (strcmp(argv[0], "expect_timeout") == 0) {
        if (argc < 3 || parse_int(argv[1], &ms) != 0 || ms < 0) {
            return -1;
        }
        subcmd = strstr(raw_cmd, argv[2]);
        if (subcmd == NULL) {
            return -1;
        }
        {
            char *buf = allocator_alloc(strlen(subcmd) + 1);
            if (buf == NULL) {
                return -1;
            }
            strcpy(buf, subcmd);
            start_ms = monotonic_ms();
            status = execute_single(buf);
            elapsed = monotonic_ms() - start_ms;
            allocator_free(buf);
        }
        if (status == 0 && elapsed < ms) {
            return 1;
        }
        if (elapsed >= ms) {
            my_print("TIMEOUT_EXPECTED\n");
            set_last_result("TIMEOUT_EXPECTED");
            return 0;
        }
        return 1;
    }

    if (strcmp(argv[0], "memstat") == 0) {
        if (argc != 1) {
            return -1;
        }
        print_memstat();
        return 0;
    }

    if (strcmp(argv[0], "memfault") == 0) {
        if (argc == 2 && strcmp(argv[1], "on") == 0) {
            allocator_set_fault_mode(1);
            set_last_result("MEMFAULT_ON");
            return 0;
        }
        if (argc == 2 && strcmp(argv[1], "off") == 0) {
            allocator_set_fault_mode(0);
            set_last_result("MEMFAULT_OFF");
            return 0;
        }
        if (argc == 3 && strcmp(argv[1], "rate") == 0) {
            rate = atof(argv[2]);
            if (allocator_set_fault_rate(rate) != 0) {
                return -1;
            }
            set_last_result("MEMFAULT_RATE_SET");
            return 0;
        }
        return -1;
    }

    return 2;
}

static int execute_builtin(char *cmd) {
    char *argv[MAX_ARGS];
    int argc;
    int bus_status;
    char *home;
    char cwd[MAX_PATH];

    argc = parse_args(cmd, argv);
    if (argc == 0) {
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0) {
        if (argc != 1) {
            return -1;
        }
        trace_close();
        exit(0);
    }
    if (strcmp(argv[0], "pwd") == 0) {
        if (argc != 1) {
            return -1;
        }
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            return -1;
        }
        my_print(cwd);
        my_print("\n");
        set_last_result(cwd);
        return 0;
    }
    if (strcmp(argv[0], "cd") == 0) {
        if (argc == 1) {
            home = getenv("HOME");
            if (home == NULL || chdir(home) != 0) {
                return -1;
            }
            set_last_result(home);
            return 0;
        }
        if (argc == 2) {
            if (chdir(argv[1]) != 0) {
                return -1;
            }
            set_last_result(argv[1]);
            return 0;
        }
        return -1;
    }

    bus_status = run_protocol_builtin(argv, argc);
    if (bus_status != 2) {
        return bus_status;
    }

    bus_status = run_control_builtin(argv, argc, cmd);
    if (bus_status != 2) {
        return bus_status;
    }

    return 2;
}

static int execute_single(char *cmd) {
    char *work;
    char *prog_part;
    char *outfile;
    int redir_type;
    int advanced = 0;
    char *argv[MAX_ARGS];
    int argc;
    int status;
    long start_ms;
    long elapsed;
    char detail[64];

    cmd = trim_whitespace(cmd);
    if (*cmd == '\0') {
        return 0;
    }

    work = allocator_alloc(strlen(cmd) + 1);
    if (work == NULL) {
        print_error();
        return 1;
    }
    strcpy(work, cmd);

    start_ms = monotonic_ms();
    redir_type = parse_redirection(work, &prog_part, &outfile, &advanced);
    if (redir_type < 0) {
        allocator_free(work);
        print_error();
        trace_log_command(cmd, 1, "bad_redirection", g_last_result, monotonic_ms() - start_ms);
        return 1;
    }

    status = execute_builtin(prog_part);
    if (status == 2) {
        argc = parse_args(prog_part, argv);
        if (argc == 0) {
            status = 0;
        } else if (redir_type == 0) {
            status = run_external(argv);
        } else {
            status = run_external_with_redirection(argv, outfile, advanced);
        }
    } else if (redir_type != 0) {
        status = -1;
    }

    if (status != 0) {
        print_error();
    }

    elapsed = monotonic_ms() - start_ms;
    (void)snprintf(detail, sizeof(detail), "redir=%d", redir_type == 0 ? 0 : (advanced ? 2 : 1));
    trace_log_command(cmd, status == 0 ? 0 : 1, detail, g_last_result, elapsed);

    allocator_free(work);
    return status == 0 ? 0 : 1;
}

static void execute_line(char *line) {
    char *commands[MAX_COMMANDS];
    int count = 0;
    char *token = strtok(line, ";");
    int i;

    while (token != NULL && count < MAX_COMMANDS) {
        commands[count++] = token;
        token = strtok(NULL, ";");
    }

    for (i = 0; i < count; i++) {
        (void)execute_single(commands[i]);
        sim_tick();
    }
}

int main(int argc, char *argv[]) {
    FILE *input = stdin;
    int interactive = 1;
    char cmd_buff[MAX_LINE];
    char *pinput;

    if (argc > 2) {
        print_error();
        return 1;
    }
    if (argc == 2) {
        interactive = 0;
        input = fopen(argv[1], "r");
        if (input == NULL) {
            print_error();
            return 1;
        }
    }

    allocator_init_from_env();
    (void)trace_init_from_env();
    (void)sim_init();
    set_last_result("");

    while (1) {
        size_t len;
        int too_long = 0;

        if (interactive) {
            my_print("myshell> ");
        }

        pinput = fgets(cmd_buff, sizeof(cmd_buff), input);
        if (pinput == NULL) {
            break;
        }

        len = strlen(cmd_buff);
        if (len == sizeof(cmd_buff) - 1 && cmd_buff[len - 1] != '\n') {
            too_long = 1;
        }

        if (!interactive) {
            (void)write(STDOUT_FILENO, cmd_buff, strlen(cmd_buff));
        }

        if (too_long) {
            print_error();
            if (interactive) {
                discard_line(input);
            }
            continue;
        }

        if (len > 0 && cmd_buff[len - 1] == '\n') {
            cmd_buff[len - 1] = '\0';
        }
        if (*cmd_buff == '\0') {
            continue;
        }

        execute_line(cmd_buff);
    }

    trace_close();
    if (!interactive) {
        fclose(input);
    }
    return 0;
}
