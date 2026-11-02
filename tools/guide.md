# Astera Aries Retimer C SDK v2.16.2: API reference

Unofficial reference for the Aries PT5161L retimer used on the Astera Labs EQUINOX-REVC card.
Generated from the Apache-2.0 Astera Labs SDK source published in Meta's OpenBMC repository
(`common/recipes-lib/retimer-v2.16.2`, commit `739d9be`). Function descriptions are the SDK
authors' own comments. This is not an Astera document and was not tested on hardware.

Not covered, because it is not public: the Equinox User's Guide, the PT5161L datasheet, the
debug header pinout, the Python SDK, and firmware images.

## Getting connected

The SDK talks to the retimer over SMBus/I2C, and the default platform layer opens `/dev/i2c-N`.
The product brief lists a **1.8 V** SMBus debug header for the retimer. The Linux `pt5161l`
driver scans addresses 0x20-0x27. `i2cDriver.slaveAddr` goes to the Linux `I2C_SLAVE` ioctl,
which takes a 7-bit address.

## Typical call flow

Condensed from the SDK's example code in `aries_common.c`. Error handling is omitted.

```c
AriesI2CDriverType i2c = {0};
AriesDeviceType dev = {0};

i2c.handle = asteraI2COpenConnection(bus, addr);
i2c.slaveAddr = addr;
i2c.pecEnable = ARIES_I2C_PEC_DISABLE;
i2c.i2cFormat = ARIES_I2C_FORMAT_ASTERA;
i2c.lockInit = 0;
dev.i2cDriver = &i2c;
dev.i2cBus = bus;
dev.partNumber = ARIES_PTX16;

ariesInitDevice(&dev, addr);   /* do not call again once FW has loaded */

AriesLinkType link[ARIES_MAX_NUM_LINKS];
ariesCreateLinkStructs(&dev, link);

for (int j = 0; j < dev.numLinks; j++) {
    ariesCheckLinkHealth(&link[j]);
    /* link[j].state.state, .rate, .recoveryCount, .linkOkay
       ARIES_STATE_FWD is the normal L0 state */
    if (!link[j].state.linkOkay)
        ariesLinkDumpDebugInfo(&link[j]);
}
```

## Signal quality measurement

```c
int o;
ariesGetPortOrientation(&dev, &o);

AriesRxMarginType m;
m.device = &dev;
m.partNumber = ARIES_PTX16;
m.orientation = o ? ARIES_ORIENTATION_REVERSED : ARIES_ORIENTATION_NORMAL;
m.do1XAnd0XCapture = true;
m.errorCountLimit = 4;

/* All 16 upstream lanes, starting at lane 0, 0.5 s dwell.
   Writes margin_test_0.csv: eye width and height per lane, plus
   recoveries, preset, FoM, DFE and IQ values. */
ariesLogEye(&m, ARIES_UP_STREAM_PSEUDO_PORT, 16, "margin_test", 0, 0.5);
```

Use `ARIES_DOWN_STREAM_PSEUDO_PORT` for the other side. `ariesEyeDiagram()` captures a full 2D
eye for one lane. The monitoring thresholds in `AriesDeviceType` (`tempAlertThreshC`,
`tempWarnThreshC`, `minLinkFoMAlert`) are SDK-side checks and do not change firmware behaviour.

## Error codes

`AriesErrorType` is defined in `aries_error.h`: `ARIES_SUCCESS` is 0 and errors are negative.
