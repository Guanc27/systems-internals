#include "sim_device.h"

#include <stdio.h>
#include <string.h>

#define STATUS_READY 0x01
#define STATUS_ERROR 0x02
#define STATUS_BUSY 0x04
#define CONTROL_START 0x01

typedef struct SimState {
    unsigned char regs[SIM_REG_COUNT];
    int busy_ticks;
    char uart_buf[256];
} SimState;

static SimState g_sim;

static int validate_endpoint(int bus, int endpoint) {
    if (bus < 0 || bus > 3) {
        return -1;
    }
    if (endpoint < 0 || endpoint > 0x7f) {
        return -1;
    }
    return 0;
}

static int validate_reg_window(int reg, int len) {
    if (reg < 0 || reg >= SIM_REG_COUNT || len <= 0 || reg + len > SIM_REG_COUNT) {
        return -1;
    }
    return 0;
}

int sim_init(void) {
    memset(&g_sim, 0, sizeof(g_sim));
    g_sim.regs[0] = STATUS_READY;
    return 0;
}

void sim_tick(void) {
    if (g_sim.busy_ticks <= 0) {
        return;
    }

    g_sim.busy_ticks--;
    if (g_sim.busy_ticks == 0) {
        g_sim.regs[0] &= (unsigned char)~STATUS_BUSY;
        g_sim.regs[0] |= STATUS_READY;
    }
}

int sim_i2c_read(int bus, int addr, int reg, int len, unsigned char *out) {
    if (validate_endpoint(bus, addr) != 0 || validate_reg_window(reg, len) != 0 || out == NULL) {
        return -1;
    }
    memcpy(out, &g_sim.regs[reg], (size_t)len);
    return 0;
}

int sim_i2c_write(int bus, int addr, int reg, const unsigned char *data, int len) {
    if (validate_endpoint(bus, addr) != 0 || validate_reg_window(reg, len) != 0 || data == NULL) {
        return -1;
    }

    memcpy(&g_sim.regs[reg], data, (size_t)len);

    if (reg <= 1 && reg + len > 1 && (g_sim.regs[1] & CONTROL_START) != 0) {
        g_sim.regs[0] &= (unsigned char)~STATUS_READY;
        g_sim.regs[0] |= STATUS_BUSY;
        g_sim.busy_ticks = 2;
        g_sim.regs[1] = 0;
    }

    return 0;
}

int sim_spi_xfer(int bus, int cs, const unsigned char *tx, int len, unsigned char *rx) {
    int i;

    if (validate_endpoint(bus, cs) != 0 || len <= 0 || tx == NULL || rx == NULL) {
        return -1;
    }

    for (i = 0; i < len; i++) {
        rx[i] = (unsigned char)(tx[i] ^ 0xA5);
    }
    return 0;
}

int sim_mdio_read(int bus, int phy, int reg, unsigned short *out) {
    if (validate_endpoint(bus, phy) != 0 || reg < 0 || reg >= 32 || out == NULL) {
        return -1;
    }
    *out = (unsigned short)((g_sim.regs[(reg * 2) % SIM_REG_COUNT] << 8) |
                            g_sim.regs[(reg * 2 + 1) % SIM_REG_COUNT]);
    return 0;
}

int sim_uart_send(int port, const char *payload) {
    if (port < 0 || port > 3 || payload == NULL) {
        return -1;
    }
    (void)snprintf(g_sim.uart_buf, sizeof(g_sim.uart_buf), "%s", payload);
    return 0;
}

const char *sim_last_uart(void) {
    return g_sim.uart_buf;
}

void sim_dump_regs(int start, int count, char *out, size_t out_len) {
    int i;
    size_t used = 0;

    if (out == NULL || out_len == 0) {
        return;
    }

    if (start < 0) {
        start = 0;
    }
    if (count <= 0) {
        count = 16;
    }
    if (start >= SIM_REG_COUNT) {
        start = SIM_REG_COUNT - 1;
    }
    if (start + count > SIM_REG_COUNT) {
        count = SIM_REG_COUNT - start;
    }

    for (i = 0; i < count; i++) {
        int n = snprintf(out + used, out_len - used, "R%02X=0x%02X%s",
                         start + i, g_sim.regs[start + i], (i + 1 == count) ? "" : " ");
        if (n < 0 || (size_t)n >= out_len - used) {
            break;
        }
        used += (size_t)n;
    }
}
