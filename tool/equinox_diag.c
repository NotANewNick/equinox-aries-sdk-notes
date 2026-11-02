/*
 * equinox_diag: command-line diagnostics for the Aries PT5161L retimer on the
 * Astera Labs EQUINOX-REVC card, built on the public Aries C SDK.
 *
 * Unofficial. Tested against a simulated chip only, never on real hardware.
 *
 *   info and links change no link or firmware settings. Every run performs
 *   the SDK's start-up sequence, whose SMBus writes select addresses, drive the
 *   firmware's read mailboxes, and run the eFuse read procedure (registers
 *   0x8EC and 0x8F6) that fetches factory temperature calibration.
 *   eye performs lane margining and requires --confirm.
 */
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aries_api.h"
#include "aries_margin.h"
#include "platform.h"

#define ARP_ADDR 0x61

static void usage(void)
{
    puts(
        "Usage: equinox_diag --bus N --addr 0x2X [options] COMMAND\n"
        "\n"
        "Commands:\n"
        "  info    Firmware, chip identity, health and temperature\n"
        "  links   State, speed, width, FoM and recovery count per link\n"
        "  eye     Measure per-lane eye margins and write CSV (needs --confirm)\n"
        "\n"
        "Options:\n"
        "  --bus N          I2C bus number, as in /dev/i2c-N (required)\n"
        "  --addr 0x2X      Retimer address, 0x20-0x27 (required)\n"
        "  --port up|down   eye: pseudo port to measure (default up)\n"
        "  --out PREFIX     eye: CSV prefix (default margin)\n"
        "  --dwell SECONDS  eye: dwell per measurement, 0-10 (default 0.5)\n"
        "  --confirm        eye: accept that margining may disturb the link\n"
        "  --verbose        Show SDK log messages\n"
        "  --help           Show this help\n"
        "\n"
        "Find the address with: i2cdetect -y -r N 0x20 0x27\n"
        "Unofficial tool, not tested on real hardware.");
}

/*
 * Replaces the SDK's weak default. If a read fails, ariesInitDevice() falls
 * back to ARP and assigns the chip a new address through 0x61. Refusing that
 * address means a wrong --addr can never reconfigure anything.
 */
int asteraI2COpenConnection(int i2cBus, int slaveAddress)
{
    char path[32];
    int fd;

    if (slaveAddress == ARP_ADDR)
    {
        fprintf(stderr, "Refusing ARP at 0x61: not used by this tool.\n");
        return -1;
    }

    snprintf(path, sizeof(path), "/dev/i2c-%d", i2cBus);
    fd = open(path, O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (asteraI2CSetSlaveAddress(fd, slaveAddress, 0) != 0)
    {
        fprintf(stderr, "Address may be in use. Is the pt5161l driver bound?\n");
        close(fd);
        return -1;
    }
    return fd;
}

static bool check_adapter(int fd)
{
    unsigned long funcs = 0;

    if (ioctl(fd, I2C_FUNCS, &funcs) < 0)
    {
        fprintf(stderr, "Cannot query adapter: %s\n", strerror(errno));
        return false;
    }
    if (!(funcs & I2C_FUNC_SMBUS_WRITE_BLOCK_DATA))
    {
        fprintf(stderr, "Adapter lacks SMBus block writes, which the SDK "
                        "requires.\n");
        return false;
    }
#ifdef SMBUS_BLOCK_READ_UNSUPPORTED
    if (!(funcs & I2C_FUNC_SMBUS_READ_I2C_BLOCK))
    {
        fprintf(stderr, "Adapter lacks I2C block reads.\n");
        return false;
    }
#else
    if (!(funcs & I2C_FUNC_SMBUS_READ_BLOCK_DATA))
    {
        fprintf(stderr, "Adapter lacks SMBus block reads. Rebuild with "
                        "'make NO_BLOCK_READ=1'.\n");
        return false;
    }
#endif
    return true;
}

static int fail(const char* what, int rc)
{
    fprintf(stderr, "%s failed (SDK error %d).\n", what, rc);
    return 1;
}

static int connect_device(AriesDeviceType* dev, AriesI2CDriverType* i2c,
                          int bus, int addr)
{
    uint8_t code = 0;
    int rc;

    memset(dev, 0, sizeof(*dev));
    memset(i2c, 0, sizeof(*i2c));

    i2c->handle = asteraI2COpenConnection(bus, addr);
    if (i2c->handle < 0)
        return 1;
    if (!check_adapter(i2c->handle))
        return 1;

    i2c->slaveAddr = addr;
    i2c->pecEnable = ARIES_I2C_PEC_DISABLE;
    i2c->i2cFormat = ARIES_I2C_FORMAT_ASTERA;
    dev->i2cDriver = i2c;
    dev->i2cBus = bus;
    dev->partNumber = ARIES_PTX16;

    rc = ariesReadByteData(i2c, ARIES_CODE_LOAD_REG, &code);
    if (rc != ARIES_SUCCESS)
    {
        fprintf(stderr,
                "No response from a retimer at 0x%02x on bus %d (error %d).\n"
                "Check the wiring, the address and that the card is powered.\n",
                addr, bus, rc);
        return 1;
    }

    rc = ariesInitDevice(dev, addr);
    if (rc != ARIES_SUCCESS)
        return fail("Device initialisation", rc);

    if (!dev->codeLoadOkay || !dev->mmHeartbeatOkay)
    {
        fprintf(stderr,
                "Firmware is not running (code load %s, heartbeat %s).\n",
                dev->codeLoadOkay ? "ok" : "BAD",
                dev->mmHeartbeatOkay ? "ok" : "BAD");
        return 1;
    }
    return 0;
}

static int cmd_info(AriesDeviceType* dev)
{
    int rc;

    printf("SDK version:   %s\n", ariesGetSDKVersion());
    printf("Firmware:      %d.%d.%d%s\n", dev->fwVersion.major,
           dev->fwVersion.minor, dev->fwVersion.build,
           dev->fwVersion.isComplianceFW ? " (compliance)" : "");
    printf("Vendor ID:     0x%04x\n", dev->vendorId);
    printf("Device ID:     0x%02x\n", dev->deviceId);
    printf("Revision:      %d\n", dev->revNumber);

    rc = ariesCheckDeviceHealth(dev);
    if (rc != ARIES_SUCCESS)
        return fail("Device health check", rc);
    printf("Device health: %s\n", dev->deviceOkay ? "OK" : "BAD");

    rc = ariesGetCurrentTemp(dev);
    if (rc == ARIES_SUCCESS)
        printf("Temperature:   %.1f C\n", dev->currentTempC);
    else
        printf("Temperature:   unavailable (SDK error %d)\n", rc);
    return dev->deviceOkay ? 0 : 1;
}

static const char* state_name(AriesLinkStateEnumType s)
{
    switch (s)
    {
        case ARIES_STATE_RESET: return "RESET";
        case ARIES_STATE_DETECT_OR_DESKEW: return "DETECT/DESKEW";
        case ARIES_STATE_FWD: return "FORWARDING (L0)";
        case ARIES_STATE_EQ_P2_0:
        case ARIES_STATE_EQ_P2_1: return "EQ PHASE 2";
        case ARIES_STATE_EQ_P3_0:
        case ARIES_STATE_EQ_P3_1: return "EQ PHASE 3";
        case ARIES_STATE_HOT_PLUG: return "HOT PLUG";
        case ARIES_STATE_OTHER: return "OTHER";
        default: return "PROTOCOL RESET";
    }
}

static int cmd_links(AriesDeviceType* dev)
{
    AriesLinkType links[ARIES_MAX_NUM_LINKS];
    int rc, i, tries, bad = 0;

    memset(links, 0, sizeof(links));
    dev->tempAlertThreshC = 110.0;
    dev->tempWarnThreshC = 100.0;
    dev->minLinkFoMAlert = 0x55;

    rc = ariesCreateLinkStructs(dev, links);
    if (rc != ARIES_SUCCESS)
        return fail("Link discovery", rc);

    for (i = 0; i < dev->numLinks; i++)
    {
        AriesLinkStateType* s = &links[i].state;

        /* Temperature sensors can report not-ready just after power-up. */
        for (tries = 0; tries < 4; tries++)
        {
            rc = ariesCheckLinkHealth(&links[i]);
            if (rc != ARIES_TEMP_READ_NOT_READY)
                break;
            sleep(1);
        }
        if (rc == ARIES_TEMP_READ_NOT_READY)
        {
            fprintf(stderr, "Temperature sensors are not ready. Try again in "
                            "a few seconds.\n");
            return 1;
        }
        if (rc != ARIES_SUCCESS)
            return fail("Link health check", rc);

        printf("Link %d: %s, Gen%d, x%d of x%d, %s\n", i, state_name(s->state),
               s->rate, s->curWidth, links[i].config.maxWidth,
               s->linkOkay ? "OK" : "PROBLEM");
        printf("  Recoveries: %d\n", s->recoveryCount);
        printf("  Minimum FoM: 0x%02x on %s\n", s->linkMinFoM,
               s->linkMinFoMRx ? s->linkMinFoMRx : "unknown");
        if (!s->linkOkay)
            bad = 1;
    }
    return bad;
}

/* Margining a link that is not in L0 would produce meaningless numbers. */
static bool links_are_up(AriesDeviceType* dev)
{
    AriesLinkType links[ARIES_MAX_NUM_LINKS];
    int i;

    memset(links, 0, sizeof(links));
    if (ariesCreateLinkStructs(dev, links) != ARIES_SUCCESS)
        return false;
    for (i = 0; i < dev->numLinks; i++)
    {
        if (ariesGetLinkState(&links[i]) != ARIES_SUCCESS)
            return false;
        if (links[i].state.state != ARIES_STATE_FWD)
        {
            fprintf(stderr, "Link %d is not up (state: %s).\n", i,
                    state_name(links[i].state.state));
            return false;
        }
    }
    return true;
}

static int cmd_eye(AriesDeviceType* dev, int up, const char* prefix,
                   double dwell)
{
    AriesPseudoPortType port = up ? ARIES_UP_STREAM_PSEUDO_PORT
                                  : ARIES_DOWN_STREAM_PSEUDO_PORT;
    AriesRxMarginType* m;
    int orientation = -1;
    int rc;

    if (!links_are_up(dev))
    {
        fprintf(stderr, "Not measuring: eye results need a trained link.\n");
        return 1;
    }

    rc = ariesGetPortOrientation(dev, &orientation);
    if (rc != ARIES_SUCCESS || (orientation != 0 && orientation != 1))
    {
        fprintf(stderr, "Could not determine port orientation.\n");
        return 1;
    }

    /* Too large for the stack comfortably: eyeResults alone is 10 KB. */
    m = calloc(1, sizeof(*m));
    if (m == NULL)
        return 1;
    m->device = dev;
    m->partNumber = ARIES_PTX16;
    m->orientation = orientation ? ARIES_ORIENTATION_REVERSED
                                 : ARIES_ORIENTATION_NORMAL;
    m->do1XAnd0XCapture = true;
    m->errorCountLimit = 4;

    printf("Measuring 16 lanes on the %s pseudo port...\n",
           up ? "upstream" : "downstream");
    rc = ariesLogEye(m, port, 16, prefix, 0, (float)dwell);
    free(m);
    if (rc != ARIES_SUCCESS)
        return fail("Eye measurement", rc);
    printf("Wrote %s_%d.csv\n", prefix, (int)port);
    return 0;
}

int main(int argc, char** argv)
{
    AriesDeviceType dev;
    AriesI2CDriverType i2c;
    const char* cmd = NULL;
    const char* prefix = "margin";
    double dwell = 0.5;
    int bus = -1, addr = -1, up = 1, confirm = 0, verbose = 0;
    int i, rc;

    for (i = 1; i < argc; i++)
    {
        const char* a = argv[i];
        const char* next = (i + 1 < argc) ? argv[i + 1] : NULL;
        char* end = NULL;

        if (!strcmp(a, "--help") || !strcmp(a, "-h"))
        {
            usage();
            return 0;
        }
        else if (!strcmp(a, "--confirm"))
            confirm = 1;
        else if (!strcmp(a, "--verbose"))
            verbose = 1;
        else if (!strncmp(a, "--", 2))
        {
            if (next == NULL)
            {
                fprintf(stderr, "%s needs a value.\n", a);
                return 2;
            }
            i++;
            if (!strcmp(a, "--bus"))
            {
                bus = (int)strtol(next, &end, 0);
                if (*end || bus < 0)
                    bus = -2;
            }
            else if (!strcmp(a, "--addr"))
            {
                addr = (int)strtol(next, &end, 0);
                if (*end)
                    addr = -2;
            }
            else if (!strcmp(a, "--port"))
            {
                if (!strcmp(next, "up"))
                    up = 1;
                else if (!strcmp(next, "down"))
                    up = 0;
                else
                {
                    fprintf(stderr, "--port must be up or down.\n");
                    return 2;
                }
            }
            else if (!strcmp(a, "--out"))
                prefix = next;
            else if (!strcmp(a, "--dwell"))
            {
                dwell = strtod(next, &end);
                if (*end || !(dwell >= 0 && dwell <= 10))
                {
                    fprintf(stderr, "--dwell must be 0 to 10 seconds.\n");
                    return 2;
                }
            }
            else
            {
                fprintf(stderr, "Unknown option %s.\n", a);
                return 2;
            }
        }
        else if (cmd == NULL)
            cmd = a;
        else
        {
            fprintf(stderr, "Unexpected argument %s.\n", a);
            return 2;
        }
    }

    if (cmd == NULL || (strcmp(cmd, "info") && strcmp(cmd, "links") &&
                        strcmp(cmd, "eye")))
    {
        usage();
        return 2;
    }
    if (bus < 0)
    {
        fprintf(stderr, "--bus is required.\n");
        return 2;
    }
    if (addr < 0x20 || addr > 0x27)
    {
        fprintf(stderr, "--addr must be 0x20 to 0x27.\n");
        return 2;
    }
    if (!strcmp(cmd, "eye") && !confirm)
    {
        fprintf(stderr,
                "eye performs lane margining, which may disturb a live link.\n"
                "Stop any important traffic and rerun with --confirm.\n");
        return 2;
    }

    asteraLogSetLevel(verbose ? 2 : 4);

    rc = connect_device(&dev, &i2c, bus, addr);
    if (rc == 0)
    {
        if (!strcmp(cmd, "info"))
            rc = cmd_info(&dev);
        else if (!strcmp(cmd, "links"))
            rc = cmd_links(&dev);
        else
            rc = cmd_eye(&dev, up, prefix, dwell);
    }

    if (i2c.handle >= 0)
        asteraI2CCloseConnection(i2c.handle);
    return rc;
}
