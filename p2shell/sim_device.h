#ifndef SIM_DEVICE_H
#define SIM_DEVICE_H

#include <stddef.h>

#define SIM_REG_COUNT 256

int sim_init(void);
void sim_tick(void);

int sim_i2c_read(int bus, int addr, int reg, int len, unsigned char *out);
int sim_i2c_write(int bus, int addr, int reg, const unsigned char *data, int len);
int sim_spi_xfer(int bus, int cs, const unsigned char *tx, int len, unsigned char *rx);
int sim_mdio_read(int bus, int phy, int reg, unsigned short *out);
int sim_uart_send(int port, const char *payload);

const char *sim_last_uart(void);
void sim_dump_regs(int start, int count, char *out, size_t out_len);

#endif
