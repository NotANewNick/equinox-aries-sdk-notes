# Astera EQUINOX-REVC: Aries SDK API Quick Reference

Astera's official Equinox documentation is not public, but its C SDK for the card's retimer chip is. The EQUINOX-REVC is a PCIe 5.0 x16 retimer card built on the Aries PT5161L, not an FPGA board. This is an unofficial summary, not an Astera document.

## What is available

| Item | Status |
| --- | --- |
| [Aries C SDK v2.16.2](https://github.com/meta/openbmc/tree/helium/common/recipes-lib/retimer-v2.16.2/files) | Public in Meta's OpenBMC repo |
| [Linux pt5161l driver](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/hwmon/pt5161l.c) | Public; temperature and status only |
| Equinox User's Guide and Python SDK | Request from info@AsteraLabs.com |
| [PT5161L datasheet](https://docs.kernel.org/next/hwmon/pt5161l.html) | Not public |

Full function reference: `aries-sdk-api-reference.md`

## Connecting

The SDK reaches the chip over SMBus/I2C, not through the PCIe data path.

- **Header:** 1.8 V SMBus debug header for the retimer. No public pinout was found.
- **Adapter:** a 1.8 V I2C adapter, such as Astera's [COMET dongle](https://www.asteralabs.com/wp-content/uploads/2020/08/Astera_Labs_COMET_Product_Brief.pdf).
- **Linux:** the default platform layer opens `/dev/i2c-N`.
- **Address:** the kernel driver scans 0x20-0x27.

## Quick start

This flow is condensed from the SDK's example code in `aries_common.c`. Error handling is omitted.

```c
AriesI2CDriverType i2c = {0};
AriesDeviceType dev = {0};

i2c.handle = asteraI2COpenConnection(bus, addr);
i2c.slaveAddr = addr;
i2c.pecEnable = ARIES_I2C_PEC_DISABLE;
i2c.i2cFormat = ARIES_I2C_FORMAT_ASTERA;
dev.i2cDriver = &i2c;
dev.i2cBus = bus;
dev.partNumber = ARIES_PTX16;

ariesInitDevice(&dev, addr);   /* do not repeat once FW has loaded */

AriesLinkType link[ARIES_MAX_NUM_LINKS];
ariesCreateLinkStructs(&dev, link);
ariesCheckLinkHealth(&link[0]);  /* then read link[0].state */
```

## Signal quality

`ariesLogEye()` is the main measurement call. It writes per-lane eye width and height to a CSV, with FoM, preset, DFE and recovery data.

```c
int o;
ariesGetPortOrientation(&dev, &o);

AriesRxMarginType m;
m.device = &dev;
m.partNumber = ARIES_PTX16;
m.orientation = o ? ARIES_ORIENTATION_REVERSED : ARIES_ORIENTATION_NORMAL;
m.do1XAnd0XCapture = true;
m.errorCountLimit = 4;

/* Writes margin_test_0.csv: 16 lanes from lane 0, 0.5 s dwell */
ariesLogEye(&m, ARIES_UP_STREAM_PSEUDO_PORT, 16, "margin_test", 0, 0.5);
```

| Task | Functions |
| --- | --- |
| Device health | `ariesCheckDeviceHealth`, `ariesFWStatusCheck`, `ariesGetCurrentTemp` |
| Link status | `ariesCheckLinkHealth`, `ariesGetLinkStateDetailed`, `ariesGetLinkRecoveryCount` |
| Debug capture | `ariesLinkDumpDebugInfo`, `ariesLTSSMLoggerInit`, `ariesLTSSMLoggerReadEntry` |
| Eye and margin | `ariesLogEye`, `ariesCheckEye`, `ariesEyeDiagram` |
| PRBS testing | `ariesTestModeEnable`, `ariesTestModeTxConfig`, `ariesTestModeRxEcountRead` |
| Raw access | `ariesReadBlockData`, `ariesWriteBlockData`, `ariesReadWideRegister` |

## Caveats

Nothing here has been tested on a card.

- **Build:** the SDK is packaged for OpenBMC, but `tool/` builds it unmodified on ordinary Linux and tests it against a simulated chip.
- **Address changes:** if a read fails, `ariesInitDevice()` falls back to ARP at 0x61, which can reassign the chip's address. Check that the chip responds first. `tool/` blocks ARP.
- **Firmware:** some features depend on the firmware version, and update images are not included.
- **Writes:** start with read-only calls. Firmware, EEPROM and test-mode calls were not checked for their effect on a live link.
