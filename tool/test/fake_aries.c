/*
 * Simulated PT5161L for tests. Loaded with LD_PRELOAD, it intercepts open() of
 * /dev/i2c-N and the I2C ioctls, so the real binary and the SMBus shim run
 * unchanged. It models the SMBus framing, code-load, heartbeat and the micro
 * indirect and mailbox handshakes. It does NOT model PCIe, links or margining,
 * so passing tests show correct software behaviour, not correct measurements.
 *
 * Environment:
 *   FAKE_ADDR=0x24   chip address        FAKE_NO_HEARTBEAT=1  firmware dead
 *   FAKE_NO_BLOCK=1  no block reads      FAKE_LOG=path        transaction log
 *   FAKE_LINK_DOWN=1 link not in L0      FAKE_BUSY=1          address in use
 *   FAKE_TEMP_NOT_READY=1  temperature sensor not ready
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "aries_globals.h"
#include "aries_a0_reg_defines.h"

#define MM_FW_INFO ARIES_MAIN_MICRO_FW_INFO
#define DMEM AL_MAIN_SRAM_DMEM_OFFSET
#define FW(off) sram[MM_FW_INFO - DMEM + (off)]
#define HEARTBEAT 0x923
#define CODE_LOAD 0x605

static uint8_t mem[0x20000], sram[0x20000];
static int fake_fd = -1, slave, rd_addr, rd_len, beat, ready;

static void init_chip(void)
{
    if (ready)
        return;
    ready = 1;
    mem[CODE_LOAD] = 0x0e;
    sram[MM_FW_INFO - DMEM + 0] = 2;  /* major */
    sram[MM_FW_INFO - DMEM + 1] = 14; /* minor */
    sram[MM_FW_INFO - DMEM + 2] = 7;  /* build */
    /* Wide register holding device and vendor IDs (test values only) */
    mem[5] = 0x42;
    mem[6] = 0xcd;
    mem[7] = 0xab;
    /* Average temperature ADC code */
    mem[0x42c] = 0x00;
    mem[0x42d] = 0x02;
    /* All-time maximum temperature ADC code */
    if (!getenv("FAKE_TEMP_NOT_READY"))
        mem[ARIES_ALL_TIME_MAX_TEMP_ADC_CSR + 1] = 0x02;
    /* GPIO0 high: firmware load complete */
    mem[0x915] = 1;

    /* A x16 Gen5 link in forwarding state, with 3 recoveries */
    int link = 0x1000, path = 38, base = link + path * 2, print = 0x2000;
    FW(ARIES_MM_LINK_STRUCT_ADDR_OFFSET) = link & 0xff;
    FW(ARIES_MM_LINK_STRUCT_ADDR_OFFSET + 1) = link >> 8;
    sram[ARIES_LINK_PATH_STRUCT_SIZE_ADDR - DMEM] = (uint8_t)path;
    sram[base + ARIES_LINK_STRUCT_WIDTH_OFFSET] = 16;
    sram[base + ARIES_LINK_STRUCT_DETECTED_WIDTH_OFFSET] = 16;
    sram[base + ARIES_LINK_STRUCT_RATE_OFFSET] = 4;
    sram[base + ARIES_LINK_STRUCT_STATE_OFFSET] =
        getenv("FAKE_LINK_DOWN") ? 5 : 6;
    FW(ARIES_MM_AL_PRINT_INFO_STRUCT_ADDR) = print & 0xff;
    FW(ARIES_MM_AL_PRINT_INFO_STRUCT_ADDR + 1) = print >> 8;
    sram[print + ARIES_PRINT_INFO_STRUCT_LNK_RECOV_ENTRIES_PTR_OFFSET] = 3;
    for (int i = 0; i < 16; i++)
        mem[ARIES_QS_0_CSR_OFFSET + ARIES_PATH_WRAPPER_1_CSR_OFFSET * i + 0xb7] =
            0x13;
}

static void logline(const char* kind, int cmd, const uint8_t* b, int n)
{
    const char* p = getenv("FAKE_LOG");
    FILE* f;
    if (!p || !(f = fopen(p, "a")))
        return;
    fprintf(f, "%s addr=%02x cmd=%02x", kind, slave, cmd);
    for (int i = 0; i < n; i++)
        fprintf(f, " %02x", b[i]);
    fputc('\n', f);
    fclose(f);
}

static void mem_write(int a, const uint8_t* d, int n)
{
    for (int i = 0; i < n; i++)
    {
        int at = a + i;
        uint8_t v = d[i];
        mem[at] = v;
        /* 0xd9d is shared: main micro indirect SRAM read, or mailbox */
        if (at == 0xd9d)
        {
            if (v == 0x16)
            {
                int src = mem[0xd99] | mem[0xd9a] << 8 | mem[0xd9b] << 16;
                mem[0xd9c] = sram[src & 0x1ffff];
            }
            else if (v >= 0x1d && v <= 0x20)
            {
                int src = mem[0xd9f] | mem[0xda0] << 8 | (mem[0xda1] & 1) << 16;
                memcpy(&mem[0xda2], &mem[src], (size_t)(v - 0x1b));
            }
            /* PMA read through the mailbox: FoM registers read a healthy 0xa0 */
            else if (v == ARIES_MM_RD_PID_IND_PMA0 || v == ARIES_MM_RD_PID_IND_PMA1)
            {
                /* The low 9 bits name the register, the rest select the lane */
                int low = (mem[0xd99] | mem[0xd9a] << 8) & 0x1ff;
                if (low == (ARIES_PMA_RAWLANE_DIG_PCS_XF_RX_ADAPT_FOM & 0x1ff))
                    mem[0xd9c] = 0xa0;
                else if (low == (ARIES_PMA_RAWLANE_DIG_PCS_XF_RX_PCS_OUT & 0x1ff))
                    mem[0xd9c] = 1; /* margining handshake ack */
                else
                    mem[0xd9c] = 0;
            }
            else if (v >= ARIES_MM_WR_WIDE_REG_2B && v <= ARIES_MM_WR_WIDE_REG_5B)
            {
                int dst = mem[0xd9f] | mem[0xda0] << 8 | (mem[0xda1] & 1) << 16;
                memcpy(&mem[dst], &mem[0xda2],
                       (size_t)(v - ARIES_MM_WR_WIDE_REG_2B + 2));
                if (getenv("FAKE_LOG"))
                {
                    FILE* f = fopen(getenv("FAKE_LOG"), "a");
                    if (f)
                    {
                        fprintf(f, "wide dst=%05x", dst);
                        for (int k = 0; k < v - ARIES_MM_WR_WIDE_REG_2B + 2; k++)
                            fprintf(f, " %02x", mem[0xda2 + k]);
                        fputc('\n', f);
                        fclose(f);
                    }
                }
            }
            mem[at] = 0;
        }
        /* Path micro indirect: writing the low address byte triggers it */
        else if (at >= 0x4202 && (at & 0xfff) == 0x202)
        {
            int cmd = mem[at - 2], len = (cmd >> 5) + 1;
            int src = (cmd >> 1 & 1) << 16 | mem[at - 1] << 8 | v;
            memcpy(&mem[at + 1], &sram[src & 0x1ffff], (size_t)len);
            mem[at + 9] = 1;
        }
    }
}

static uint8_t mem_read(int a)
{
    if (a == HEARTBEAT)
        return getenv("FAKE_NO_HEARTBEAT") ? 0x55 : (uint8_t)(beat++);
    return mem[a];
}

static int smbus(struct i2c_smbus_ioctl_data* x)
{
    int want = (int)strtol(getenv("FAKE_ADDR") ? getenv("FAKE_ADDR") : "0x24",
                           NULL, 0);
    uint8_t* b = x->data ? x->data->block : NULL;

    init_chip();
    if (slave != want)
    {
        logline("nak", x->command, NULL, 0);
        errno = ENXIO;
        return -1;
    }
    if (x->read_write == I2C_SMBUS_WRITE && x->size == I2C_SMBUS_BLOCK_DATA)
    {
        int n = b[0];
        logline("wr", x->command, &b[1], n);
        if (x->command == 0x0f && n >= 4)
            mem_write((b[1] & 1) << 16 | b[2] << 8 | b[3], &b[4], n - 3);
        else if (x->command == 0x0a && n == 3)
        {
            rd_addr = (b[1] & 1) << 16 | b[2] << 8 | b[3];
            rd_len = ((b[1] >> 1) & 0xf) + 1;
        }
        else
        {
            errno = EPROTO;
            return -1;
        }
        return 0;
    }
    if (x->read_write == I2C_SMBUS_READ && x->size == I2C_SMBUS_BLOCK_DATA)
    {
        if (getenv("FAKE_NO_BLOCK"))
        {
            errno = EOPNOTSUPP;
            return -1;
        }
        b[0] = (uint8_t)rd_len;
        for (int i = 0; i < rd_len; i++)
            b[1 + i] = mem_read(rd_addr + i);
        logline("rd", x->command, &b[1], rd_len);
        return 0;
    }
    if (x->read_write == I2C_SMBUS_READ && x->size == I2C_SMBUS_I2C_BLOCK_DATA)
    {
        /* Raw transfer: the device's length byte arrives as the first byte */
        int n = b[0];
        b[1] = (uint8_t)rd_len;
        for (int i = 0; i < n - 1; i++)
            b[2 + i] = mem_read(rd_addr + i);
        logline("rdi", x->command, &b[1], n);
        return 0;
    }
    errno = EOPNOTSUPP;
    return -1;
}

int open(const char* path, int flags, ...)
{
    static int (*real)(const char*, int, ...);
    va_list ap;
    int mode;
    if (!real)
        real = dlsym(RTLD_NEXT, "open");
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
    if (!strncmp(path, "/dev/i2c-", 9))
    {
        if (atoi(path + 9) != 3)
        {
            errno = ENOENT;
            return -1;
        }
        return fake_fd = real("/dev/null", O_RDWR);
    }
    return real(path, flags, mode);
}

int ioctl(int fd, unsigned long req, ...)
{
    static int (*real)(int, unsigned long, ...);
    va_list ap;
    void* arg;
    if (!real)
        real = dlsym(RTLD_NEXT, "ioctl");
    va_start(ap, req);
    arg = va_arg(ap, void*);
    va_end(ap);
    if (fd != fake_fd)
        return real(fd, req, arg);
    switch (req)
    {
        case I2C_SLAVE:
            if (getenv("FAKE_BUSY"))
            {
                errno = EBUSY;
                return -1;
            }
            slave = (int)(long)arg;
            return 0;
        case I2C_FUNCS:
            *(unsigned long*)arg = I2C_FUNC_I2C |
                                   I2C_FUNC_SMBUS_WRITE_BLOCK_DATA |
                                   I2C_FUNC_SMBUS_READ_I2C_BLOCK;
            if (!getenv("FAKE_NO_BLOCK"))
                *(unsigned long*)arg |= I2C_FUNC_SMBUS_READ_BLOCK_DATA;
            return 0;
        case I2C_SMBUS: return smbus(arg);
    }
    errno = ENOTTY;
    return -1;
}
