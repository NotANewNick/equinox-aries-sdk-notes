/*
 * Stand-in for OpenBMC's <openbmc/obmc-i2c.h>, so the unmodified Aries SDK
 * builds on ordinary Linux with no extra libraries. It provides the SMBus
 * helpers the SDK calls, using the kernel's I2C_SMBUS ioctl directly.
 */
#ifndef EQUINOX_COMPAT_OBMC_I2C_H
#define EQUINOX_COMPAT_OBMC_I2C_H

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static inline int compat_smbus(int fd, char rw, uint8_t cmd, int size,
                               union i2c_smbus_data* data)
{
    struct i2c_smbus_ioctl_data args = {
        .read_write = rw, .command = cmd, .size = (uint32_t)size, .data = data};
    return ioctl(fd, I2C_SMBUS, &args) < 0 ? -errno : 0;
}

static inline int i2c_smbus_write_block_data(int fd, uint8_t cmd, uint8_t len,
                                             const uint8_t* buf)
{
    union i2c_smbus_data d;
    if (len > I2C_SMBUS_BLOCK_MAX)
        return -EINVAL;
    d.block[0] = len;
    memcpy(&d.block[1], buf, len);
    return compat_smbus(fd, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_BLOCK_DATA, &d);
}

static inline int i2c_smbus_read_block_data(int fd, uint8_t cmd, uint8_t* buf)
{
    union i2c_smbus_data d;
    int rc = compat_smbus(fd, I2C_SMBUS_READ, cmd, I2C_SMBUS_BLOCK_DATA, &d);
    if (rc < 0)
        return rc;
    memcpy(buf, &d.block[1], d.block[0]);
    return d.block[0];
}

static inline int i2c_smbus_read_i2c_block_data(int fd, uint8_t cmd,
                                                uint8_t len, uint8_t* buf)
{
    union i2c_smbus_data d;
    int rc;
    if (len > I2C_SMBUS_BLOCK_MAX)
        len = I2C_SMBUS_BLOCK_MAX;
    d.block[0] = len;
    rc = compat_smbus(fd, I2C_SMBUS_READ, cmd, I2C_SMBUS_I2C_BLOCK_DATA, &d);
    if (rc < 0)
        return rc;
    memcpy(buf, &d.block[1], d.block[0]);
    return d.block[0];
}

static inline int i2c_smbus_block_process_call(int fd, uint8_t cmd, uint8_t len,
                                               uint8_t* buf)
{
    union i2c_smbus_data d;
    int rc;
    if (len > I2C_SMBUS_BLOCK_MAX)
        return -EINVAL;
    d.block[0] = len;
    memcpy(&d.block[1], buf, len);
    rc = compat_smbus(fd, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_BLOCK_PROC_CALL, &d);
    if (rc < 0)
        return rc;
    memcpy(buf, &d.block[1], d.block[0]);
    return d.block[0];
}

#endif
