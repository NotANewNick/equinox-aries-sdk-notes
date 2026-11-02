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

## Core API: init, health, firmware, EEPROM, GPIO, PRBS test mode (`aries_api.h`)

### `ariesGetSDKVersion`

```c
const char* ariesGetSDKVersion(void);
```

Return the SDK version

Returns: const char* - SDK version number

### `ariesInitDevice`

```c
AriesErrorType ariesInitDevice(AriesDeviceType* device, uint8_t recoveryAddr);
```

Initialize Aries device data structure Capture the FW version, device id, vendor id and revision id and store the values in the device struct BMC should wait until the Retimer has had the opportunity to exit reset and load firmware before initializing the ariesDevice object with the ariesInitDevice() API. This can be accomplished by any of the following: - Wait until the device’s GPIO0 pin is high, indicating FW load is complete - Wait >1.8 seconds after the device’s RESET_N pin is de-asserted low and REFCLK is present - Check with the ariesFWStatusCheck(AriesDeviceType* device) API If the BMC cannot implement these checks, then BMC can poll for device ready status purely in software: - Run the ariesInitDevice() API - If FW has not yet loaded successfully, then wait for 2 seconds before calling ariesInitDevice() again. FW load successful is indicated by: + device->mmHeartbeatOkay, and + (device->fwVersion.major != 0) || (device->fwVersion.minor != 0) || (device->fwVersion.build != 0) - Do not call ariesInitDevice() after it has been established that FW is loaded, as this would be a redundant - ariesInitDevice() need not (and should not) be called again after it is determined FW has loaded. Device monitoring can proceed once it is established that FW has loaded successfully.

- `device`: Aries device struct
- `recoveryAddr`: Desired I2C (7-bit) address in case ARP needs to be run

Returns: AriesErrorType - Aries error code

### `ariesCreateLinkStructs`

```c
AriesErrorType ariesCreateLinkStructs(AriesDeviceType* device, AriesLinkType* links);
```

Initializes array of link structs Takes an array of AriesLinkType structs and populates the first N indices for each link the device has been bifurcated into via FW (max 8 links per device). Also returns the number of links for the device's bifurcation.

- `device`: Aries device struct
- `links`: Array of link structs to be populated

Returns: AriesErrorType - Aries error code

### `ariesFWStatusCheck`

```c
AriesErrorType ariesFWStatusCheck(AriesDeviceType* device);
```

Check status of FW loaded Check the code load register and the Main Micro heartbeat. If all is good, then read the firmware version

- `device`: Aries device struct

Returns: AriesErrorType - Aries error code

### `ariesDeviceRevisionGet`

```c
AriesErrorType ariesDeviceRevisionGet(AriesDeviceType* device);
```

Get the retimer device revsion Updates the AriesDevice struct revNumber variable 0 = MPW|"A-Step", 1 = A0|"B-Step", 2 = B0|"C-Step"

- `device`: Aries device struct

Returns: AriesErrorType - Aries error code

### `ariesComplianceFWGet`

```c
AriesErrorType ariesComplianceFWGet(AriesDeviceType* device);
```

Checks if Aries device is running compliance FW Checks the ate_customer_board and self_test compile options to determine if compliance FW is running on the device, rather than mission-mode FW.

- `device`: Aries device struct

Returns: AriesErrorType - Aries error code

### `ariesSetEEPROMPageSize`

```c
AriesErrorType ariesSetEEPROMPageSize(AriesDeviceType* device, int pageSize);
```

Update the EEPROM page variables This is only necessariy if a 512 kbit EEPROM is being used

- `device`: Aries device struct
- `pageSize`: Page size of EEPROMs in bytes

Returns: AriesErrorType - Aries error code

### `ariesSetBifurcationMode`

```c
AriesErrorType ariesSetBifurcationMode(AriesDeviceType* device, AriesBifurcationType bifur);
```

Set the bifurcation mode Bifurcation mode is written to global parameter register 0. Returns a negative error code, else zero on success.

- `device`: Struct containing device information
- `bifur`: Bifurcation mode

Returns: AriesErrorType - Aries error code

### `ariesGetBifurcationMode`

```c
AriesErrorType ariesGetBifurcationMode(AriesDeviceType* device, AriesBifurcationType* bifur);
```

Get the bifurcation mode Bifurcation mode is read from the global parameter register 0. Returns a negative error code, else zero on success.

- `device`: Struct containing device information
- `bifur`: Pointer to bifurcation mode variable

Returns: AriesErrorType - Aries error code

### `ariesSetHwReset`

```c
AriesErrorType ariesSetHwReset(AriesDeviceType* device, uint8_t reset);
```

Set the Retimer HW Reset. This function sets the register to assert reset (1) or de-assert reset (0). Asserting reset would set the retimer in reset, and de-asserting it will bring the retimer out of reset and cause a firmware reload. User must wait 1ms between assertion and de-assertion

- `device`: Struct containing device information
- `reset`: Reset assert (1) or de-assert (0)

Returns: AriesErrorType - Aries error code

### `ariesSetMMI2CMasterReset`

```c
AriesErrorType ariesSetMMI2CMasterReset(AriesDeviceType* device, uint8_t reset);
```

Set the Main Micro and I2C Master Reset. This function sets the register to assert reset (1) or de-assert reset (0). Asserting the MM reset will halt firmware execution and de-asserting will cause firmware to reload Asserting the I2C master reset can be used to keep the Retimer from sending transactions on the I2C bus connected to the EEPROM

- `device`: Struct containing device information
- `reset`: Reset assert (1) or de-assert (0)

Returns: AriesErrorType - Aries error code

### `ariesSetI2CMasterReset`

```c
AriesErrorType ariesSetI2CMasterReset(AriesDeviceType* device, uint8_t reset);
```

Set the I2C Master Reset. This function sets the register to assert reset (1) or de-assert reset (0). Asserting the I2C master reset can be used to keep the Retimer from sending tranactions on the I2C bus connected to the EEPROM

- `device`: Struct containing device information
- `reset`: Reset assert (1) or de-assert (0)

Returns: AriesErrorType - Aries error code

### `ariesUpdateFirmware`

```c
AriesErrorType ariesUpdateFirmware(AriesDeviceType* device, const char* filename, AriesFWImageFormatType fileType);
```

Update the FW image in the EEPROM connected to the Retimer. The firmware image specififed by filename will be written to the EEPROM attached to the Retimer then verified using the optimal method.

- `device`: Struct containing device information
- `filename`: Filename of the file containing the firmware
- `fileType`: Enum specifying firmware image file type (IHX or BIN)

Returns: AriesErrorType - Aries error code

### `ariesUpdateFirmwareViaBuffer`

```c
AriesErrorType ariesUpdateFirmwareViaBuffer(AriesDeviceType* device, uint8_t* image);
```

Update the FW image in the EEPROM connected to the Retimer via buffer The firmware image preloaded into a buffer will be written to the EEPROM attached to the Retimer then verified using the optimal method.

- `device`: Struct containing device information
- `image`: Pointer to buffer of bytes containing image

Returns: AriesErrorType - Aries error code

### `ariesFirmwareUpdateProgress`

```c
AriesErrorType ariesFirmwareUpdateProgress(AriesDeviceType* device, uint8_t* percentComplete);
```

Get the progress of the FW update in percent complete.

- `device`: Struct containing device information
- `percentComplete`: Pointer to percent complete variable

Returns: AriesErrorType - Aries error code

### `ariesWriteEEPROMImage`

```c
AriesErrorType ariesWriteEEPROMImage(AriesDeviceType* device, uint8_t* values, bool legacyMode);
```

Load a FW image into the EEPROM connected to the Retimer. numBytes from the values[] buffer will be written to the EEPROM attached to the Retimer.

- `device`: Struct containing device information
- `values`: Pointer to byte array containing the data to be written to the EEPROM
- `legacyMode`: If true, write EEPROM in slower legacy mode (set this flag in error scenarios, when you wish to write EEPROM without using faster Main Micro assisted writes). Please set to false otherwise.

Returns: AriesErrorType - Aries error code

### `ariesVerifyEEPROMImage`

```c
AriesErrorType ariesVerifyEEPROMImage(AriesDeviceType* device, uint8_t* values, bool legacyMode);
```

Verify the FW image in the EEPROM connected to the Retimer. numBytes from the values[] buffer will be compared against the contents of the EEPROM attached to the Retimer. Returns a negative error code, else zero on success.

- `device`: Struct containing device information
- `values`: Pointer to byte array containing the expected firmware image. The actual contents of the EEPROM will be compared against this.
- `legacyMode`: If true, write EEPROM in slower legacy mode (set this flag in error scenarios, when you wish to read EEPROM without using faster Main Micro assisted read). Please set to false otherwise.

Returns: AriesErrorType - Aries error code

### `ariesVerifyEEPROMImageViaChecksum`

```c
AriesErrorType ariesVerifyEEPROMImageViaChecksum(AriesDeviceType* device, uint8_t* image);
```

Verify the FW image in the EEPROM connected to the Retimer via checksum. In this case, no re-writes happen in case of a failure. It is recommended to attempt the rewrite the FW into the EEPROM again upon failure

- `device`: Struct containing device information
- `image`: Pointer to byte array containing the expected firmware image. The actual contents of the EEPROM will be compared against this.

Returns: AriesErrorType - Aries error code

### `ariesCheckEEPROMCrc`

```c
AriesErrorType ariesCheckEEPROMCrc(AriesDeviceType* device, uint8_t* image);
```

Calculate block CRCs from data in EEPROM

- `device`: Struct containing device information
- `image`: Array containing FW info (in bytes)

Returns: AriesErrorType - Aries error code

### `ariesCheckEEPROMImageCrcBytes`

```c
AriesErrorType ariesCheckEEPROMImageCrcBytes(AriesDeviceType* device, uint8_t* crcBytes, uint8_t* numCrcBytes);
```

Calculate block CRCs from data in EEPROM

- `device`: Struct containing device information
- `crcBytes`: Array containing block crc bytes
- `numCrcBytes`: size of crcBytes (in bytes)

Returns: AriesErrorType - Aries error code

### `ariesReadEEPROMByte`

```c
AriesErrorType ariesReadEEPROMByte(AriesDeviceType* device, int addr, uint8_t* value);
```

Read a byte from the EEPROM. Executes the sequence of SMBus transactions to read a single byte from the EEPROM connected to the retimer.

- `device`: Struct containing the device information
- `addr`: EEPROM address to read a byte from
- `value`: Data read from EEPROM (1 byte)

Returns: AriesErrorType - Aries error code

### `ariesWriteEEPROMByte`

```c
AriesErrorType ariesWriteEEPROMByte(AriesDeviceType* device, int addr, uint8_t* value);
```

Write a byte to the EEPROM. Executes the sequence of SMBus transactions to write a single byte to the EEPROM connected to the retimer.

- `device`: Struct containing the device information
- `addr`: EEPROM address to write a byte to
- `value`: Data to write to EEPROM (1 byte)

Returns: AriesErrorType - Aries error code

### `ariesCheckConnectionHealth`

```c
AriesErrorType ariesCheckConnectionHealth(AriesDeviceType* device, uint8_t slaveAddress);
```

Check connection health

- `device`: Pointer to Aries Device struct object
- `slaveAddress`: Desired Retimer I2C (7-bit) address in case ARP needs to be run

Returns: AriesErrorType - Aries error code

### `ariesCheckDeviceHealth`

```c
AriesErrorType ariesCheckDeviceHealth(AriesDeviceType* device);
```

Perform an all-in-one health check on the device. Check if regular accesses to the EEPROM are working, and if the EEPROM has loaded correctly (i.e. Main Mircocode, Path Microcode and PMA code have loaded correctly).

- `device`: Pointer to Aries Device struct object

Returns: AriesErrorType - Aries error code

### `ariesGetMaxTemp`

```c
AriesErrorType ariesGetMaxTemp(AriesDeviceType* device);
```

Get the max recorded junction temperature. Read the maximum junction temperature and return in units of degrees Celsius. This value represents the maximum value from all temperature sensors on the Retimer. Returns a negative error code, else zero on success.

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

### `ariesGetCurrentTemp`

```c
AriesErrorType ariesGetCurrentTemp(AriesDeviceType* device);
```

Get the current junction temperature. Read the current junction temperature and return in units of degrees Celsius.

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

### `ariesSetMaxDataRate`

```c
AriesErrorType ariesSetMaxDataRate(AriesDeviceType* device, AriesMaxDataRateType rate);
```

Set max data rate

- `device`: Struct containing device information
- `rate`: Max data rate to set

Returns: AriesErrorType - Aries error code

### `ariesSetGPIO`

```c
AriesErrorType ariesSetGPIO(AriesDeviceType* device, int gpioNum, bool value);
```

Set the GPIO value

- `device`: Struct containing device information
- `gpioNum`: GPIO number [0:3]
- `value`: GPIO value (0) or (1)

Returns: AriesErrorType - Aries error code

### `ariesGetGPIO`

```c
AriesErrorType ariesGetGPIO(AriesDeviceType* device, int gpioNum, bool* value);
```

Get the GPIO value

- `device`: Struct containing device information
- `gpioNum`: GPIO number [0:3]
- `value`: Pointer to GPIO value (0) or (1) variable

Returns: AriesErrorType - Aries error code

### `ariesToggleGPIO`

```c
AriesErrorType ariesToggleGPIO(AriesDeviceType* device, int gpioNum);
```

Toggle the GPIO value

- `device`: Struct containing device information
- `gpioNum`: GPIO number [0:3]

Returns: AriesErrorType - Aries error code

### `ariesSetGPIODirection`

```c
AriesErrorType ariesSetGPIODirection(AriesDeviceType* device, int gpioNum, bool value);
```

Set the GPIO direction

- `device`: Struct containing device information
- `gpioNum`: GPIO number [0:3]
- `value`: GPIO direction (0 = output) or (1 = input)

Returns: AriesErrorType - Aries error code

### `ariesGetGPIODirection`

```c
AriesErrorType ariesGetGPIODirection(AriesDeviceType* device, int gpioNum, bool* value);
```

Get the GPIO direction

- `device`: Struct containing device information
- `gpioNum`: GPIO number [0:3]
- `value`: Pointer to GPIO direction (0 = output) or (1 = input) variable

Returns: AriesErrorType - Aries error code

### `ariesTestModeEnable`

```c
AriesErrorType ariesTestModeEnable(AriesDeviceType* device);
```

Enable Aries Test Mode for PRBS generation and checking

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

### `ariesTestModeDisable`

```c
AriesErrorType ariesTestModeDisable(AriesDeviceType* device);
```

Disable Aries Test Mode for PRBS generation and checking

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

### `ariesTestModeRateChange`

```c
AriesErrorType ariesTestModeRateChange(AriesDeviceType* device, AriesMaxDataRateType rate);
```

Set the desired Aries Test Mode data rate

- `device`: Struct containing device information
- `rate`: Desired rate (1, 2, ... 5)

Returns: AriesErrorType - Aries error code

### `ariesTestModeTxConfig`

```c
AriesErrorType ariesTestModeTxConfig(AriesDeviceType* device, AriesPRBSPatternType pattern, int preset, bool enable);
```

Aries Test Mode transmitter configuration

- `device`: Struct containing device information
- `pattern`: Desired PRBS data pattern
- `preset`: Desired Tx preset setting
- `enable`: Enable (1) or disable (0) flag

Returns: AriesErrorType - Aries error code

### `ariesTestModeTxConfigLane`

```c
AriesErrorType ariesTestModeTxConfigLane(AriesDeviceType* device, AriesPRBSPatternType pattern, int preset, int side, int lane, bool enable);
```

Aries Test Mode transmitter single lane configuration

- `device`: Struct containing device information
- `pattern`: Desired PRBS data pattern
- `preset`: Desired Tx preset setting
- `enable`: Enable (1) or disable (0) flag
- `side`: Side of chip
- `lane`: Lane to configure

Returns: AriesErrorType - Aries error code

### `ariesTestModeRxConfig`

```c
AriesErrorType ariesTestModeRxConfig(AriesDeviceType* device, AriesPRBSPatternType pattern, bool enable);
```

Aries Test Mode receiver configuration

- `device`: Struct containing device information
- `pattern`: Desired PRBS data pattern
- `enable`: Enable (1) or disable (0) flag

Returns: AriesErrorType - Aries error code

### `ariesTestModeRxEcountRead`

```c
AriesErrorType ariesTestModeRxEcountRead(AriesDeviceType* device, int* ecount);
```

Aries Test Mode read error count

- `device`: Struct containing device information
- `ecount`: Array containing error count data for each side and lane

Returns: AriesErrorType - Aries error code

### `ariesTestModeRxEcountClear`

```c
AriesErrorType ariesTestModeRxEcountClear(AriesDeviceType* device);
```

Aries Test Mode clear error count Reads ecount values for each side and lane and populates an array

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

### `ariesTestModeRxEcountClearLane`

```c
AriesErrorType ariesTestModeRxEcountClearLane(AriesDeviceType* device, int side, int lane);
```

Aries Test Mode clear single lane error count Reads ecount values for a single lane on one side and populates an array

- `device`: Struct containing device information
- `side`: Side of chip
- `lane`: Lane to clear error count for

Returns: AriesErrorType - Aries error code

### `ariesTestModeRxFomRead`

```c
AriesErrorType ariesTestModeRxFomRead(AriesDeviceType* device, int* fom);
```

Aries Test Mode read FoM Reads FoM values for each side and lane and populates an array

- `device`: Struct containing device information
- `fom`: Array containing FoM data for each side and lane

Returns: AriesErrorType - Aries error code

### `ariesTestModeRxValidRead`

```c
AriesErrorType ariesTestModeRxValidRead(AriesDeviceType* device);
```

Aries Test Mode read Rx valid

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

### `ariesTestModeTxErrorInject`

```c
AriesErrorType ariesTestModeTxErrorInject(AriesDeviceType* device);
```

Aries Test Mode inject error

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

### `ariesFWReloadOnNextSBR`

```c
AriesErrorType ariesFWReloadOnNextSBR(AriesDeviceType* device);
```

Aries trigger FW reload on next secondary bus reset (SBR) This only works on Firmware version 1.28.x or later.

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

### `ariesOvertempStatusGet`

```c
AriesErrorType ariesOvertempStatusGet(AriesDeviceType* device, bool* value);
```

Aries get over temperature status This only works on Firmware version 1.28.x or later.

- `device`: Struct containing device information
- `value`: Pointer to bool with over temp status result

Returns: AriesErrorType - Aries error code

### `ariesGetI2CBusClearEventStatus`

```c
AriesErrorType ariesGetI2CBusClearEventStatus(AriesDeviceType* device, int* status);
```

Aries get status of I2C bus self-clear event log When an errant I2C transaction (e.g. a transaction intervening between the Write and Read portions of a Aries Read transaction, or a transaction which does not follow the specified transaction format) causes the Aries I2C slave to get into a stuck state, the Aries FW will detect and clear this state. This API reads back status to show if such an event has occured (1) or not (0). This is a sticky status.

- `device`: Struct containing device information
- `status`: Pointer to return value

Returns: AriesErrorType - Aries error code

### `ariesClearI2CBusClearEventStatus`

```c
AriesErrorType ariesClearI2CBusClearEventStatus(AriesDeviceType* device);
```

Aries clear I2C bus self-clear event log status

- `device`: Struct containing device information

Returns: AriesErrorType - Aries error code

## Link status (`aries_link.h`)

### `ariesSetPcieReset`

```c
AriesErrorType ariesSetPcieReset(AriesLinkType* link, uint8_t reset);
```

Set the PCIe Protocol Reset. PCIe Protocol (fundamental) Reset may be triggered from the PERST_N hardware pin or from a register. This function sets the register to assert PCIe reset (1) or de-assert PCIe reset (0). User must wait 1 ms between an assertion and de-assertion.

- `link`: Link object (containing link id)
- `reset`: Reset assert (1) or de-assert (0)

Returns: AriesErrorType - Aries error code

### `ariesCheckLinkHealth`

```c
AriesErrorType ariesCheckLinkHealth(AriesLinkType* link);
```

Perform an all-in-one health check on a given Link. Check critical parameters like junction temperature, Link LTSSM state, and per-lane eye height/width against certain alert thresholds. Update link.linkOkay member (bool).

- `link`: Pointer to Aries Link struct object

Returns: AriesErrorType - Aries error code

### `ariesLinkGetCurrentWidth`

```c
AriesErrorType ariesLinkGetCurrentWidth(AriesLinkType* link, int* currentWidth);
```

Read the current width of the link

- `link`: Pointer to Aries Link struct object
- `currentWidth`: Pointer to integer to store the current link width

Returns: AriesErrorType - Aries error code

### `ariesGetLinkRecoveryCount`

```c
AriesErrorType ariesGetLinkRecoveryCount(AriesLinkType* link, int* recoveryCount);
```

Get link recovery counter value

- `link`: Link struct created by user
- `recoveryCount`: Recovery count returned

Returns: AriesErrorType - Aries error code

### `ariesClearLinkRecoveryCount`

```c
AriesErrorType ariesClearLinkRecoveryCount(AriesLinkType* link);
```

Clear link recovery counter value

- `link`: Link struct created by user

Returns: AriesErrorType - Aries error code

### `ariesGetLinkState`

```c
AriesErrorType ariesGetLinkState(AriesLinkType* link);
```

Get the current detailed Link state, including electrical parameters. Read the current Link state and return the parameters for the current links Returns a negative error code, else zero on success.

- `link`: Pointer to Aries Link struct object

Returns: AriesErrorType - Aries error code

### `ariesGetLinkStateDetailed`

```c
AriesErrorType ariesGetLinkStateDetailed(AriesLinkType* link);
```

Get the current detailed Link state, including electrical parameters. Read the current Link state and return the parameters for the current links Returns a negative error code, else zero on success.

- `link`: Pointer to Aries Link struct object

Returns: AriesErrorType - Aries error code

### `ariesLTSSMLoggerInit`

```c
AriesErrorType ariesLTSSMLoggerInit(AriesLinkType* link, uint8_t oneBatchModeEn, AriesLTSSMVerbosityType verbosity);
```

Initialize LTSSM logger. Configure the LTSSM logger for one-batch or continuous mode and set the print classes (and per-Link enables for main microcontroller log). Returns a negative error code, else zero on success.

- `link`: Pointer to Aries Link struct object
- `oneBatchModeEn`: Enable one-batch mode (1) or continuous mode (0)
- `verbosity`: Logger verbosity control

Returns: AriesErrorType - Aries error code

### `ariesLTSSMLoggerPrintEn`

```c
AriesErrorType ariesLTSSMLoggerPrintEn(AriesLinkType* link, uint8_t printEn);
```

Enable or disable LTSSM logger.

- `link`: Pointer to Aries Link struct object
- `printEn`: Enable (1) or disable (0) printing for this Link

Returns: AriesErrorType - Aries error code

### `ariesLTSSMLoggerClear`

```c
AriesErrorType ariesLTSSMLoggerClear(AriesLinkType* link);
```

Clear LTSSM Logger

- `link`: Pointer to Aries Link struct object

Returns: AriesErrorType - Aries error code

### `ariesLTSSMLoggerReadEntry`

```c
AriesErrorType ariesLTSSMLoggerReadEntry(AriesLinkType* link, AriesLTSSMLoggerEnumType logType, int* offset, AriesLTSSMEntryType* entry);
```

Read an entry from the LTSSM logger. The LTSSM log entry starting at offset is returned, and offset is updated to point to the next entry. Returns a negative error code, including ARIES_LTSSM_INVALID_ENTRY if the the end of the log is reached, else zero on success.

- `link`: Pointer to Aries Link struct object
- `logType`: The specific log to read from
- `offset`: Pointer to the log offset value
- `entry`: Pointer to Aries LTSSM Logger Entry struct returned by this function

Returns: AriesErrorType - Aries error code

### `ariesLinkDumpDebugInfo`

```c
AriesErrorType ariesLinkDumpDebugInfo(AriesLinkType* link);
```

Dump all the debug info from the retimer

- `link`: Pointer to Aries Link struct object

Returns: AriesErrorType - Aries error code

### `ariesLinkPrintMicroLogs`

```c
AriesErrorType ariesLinkPrintMicroLogs(AriesLinkType* link, const char* basepath, const char* filename);
```

Print logs from all micros in retimer to a file

- `link`: Pointer to Aries Link struct object
- `basepath`: root directory path
- `filename`: name of file to write micro logs to

Returns: AriesErrorType - Aries error code

### `ariesLinkPrintDetailedState`

```c
AriesErrorType ariesLinkPrintDetailedState(AriesLinkType* link, const char* basepath, const char* filename);
```

Capture the detailed link state and print it to file

- `link`: Pointer to Aries Link struct object
- `basepath`: root directory path
- `filename`: name of file to write micro logs to

Returns: AriesErrorType - Aries error code

### `ariesPrintLog`

```c
AriesErrorType ariesPrintLog(AriesLinkType* link, AriesLTSSMLoggerEnumType log, FILE* fp);
```

Print the micro logger entries Low level api used by ariesLinkPrintMicroLogs() not generally used directly

- `link`: Pointer to Aries Link struct object
- `log`: log type
- `fp`: file pointer to output file for logger entries

Returns: AriesErrorType - Aries error code

### `ariesLinkPresetRequestListSet`

```c
AriesErrorType ariesLinkPresetRequestListSet(AriesLinkType* link, AriesPseudoPortType port, AriesMaxDataRateType rate, uint16_t laneMask, uint16_t presetOneHotList);
```

Set the list of presets that the retimer should request This modifies the list of presets that the retimer will request when performing Phase 2 and 3 of Equalization

- `link`: Pointer to Aries Link struct object
- `port`: Port to apply new settings to (USPP or DSPP)
- `rate`: PCIe rate to apply settings to (Gen-3, Gen-4, or Gen-5)
- `laneMask`: Logical lane mask of lanes in the link to update
- `presetOneHotList`: The one-hot preset request list; set the bit corresponding to the preset you want to enable; for example to request presets 4, 5, and 9 the presetOneHotList would be 0x230

Returns: AriesErrorType - Aries error code

## Lane margining and eye measurement (`aries_margin.h`)

### `ariesMarginNoCommand`

```c
AriesErrorType ariesMarginNoCommand(AriesRxMarginType* marginDevice);
```

Margin Command for No Command Sending this out of band doesn't do anything

- `marginDevice`: Struct containing Margin Device information

Returns: AriesErrorType - Aries error code

### `ariesMarginAccessRetimerRegister`

```c
AriesErrorType ariesMarginAccessRetimerRegister(AriesRxMarginType* marginDevice);
```

Margin Command to access Retimer register Register access is not implemented yet

- `marginDevice`: Struct containing Margin Device information

Returns: AriesErrorType - Aries error code

### `ariesMarginReportMarginControlCapabilities`

```c
AriesErrorType ariesMarginReportMarginControlCapabilities(AriesRxMarginType* marginDevice, int* capabilities);
```

Margin Command to print and store margin control capabilities

- `marginDevice`: Struct containing Margin Device information
- `capabilities`: Array to store margin device capabilities

Returns: AriesErrorType - Aries error code

### `ariesMarginReportNumVoltageSteps`

```c
AriesErrorType ariesMarginReportNumVoltageSteps(AriesRxMarginType* marginDevice, int* numVoltageSteps);
```

Margin Command to report the number of voltage steps

- `marginDevice`: Struct containing Margin Device information
- `numVoltageSteps`: variable to store number of voltage steps in

Returns: AriesErrorType - Aries error code

### `ariesMarginReportNumTimingSteps`

```c
AriesErrorType ariesMarginReportNumTimingSteps(AriesRxMarginType* marginDevice, int* numTimingSteps);
```

Margin Command to report the number of timing steps

- `marginDevice`: Struct containing Margin Device information
- `numTimingSteps`: variable to store number of timing steps in

Returns: AriesErrorType - Aries error code

### `ariesMarginReportMaxTimingOffset`

```c
AriesErrorType ariesMarginReportMaxTimingOffset(AriesRxMarginType* marginDevice, int* maxTimingOffset);
```

Margin Command to report the max timing offset

- `marginDevice`: Struct containing Margin Device information
- `maxTimingOffset`: variable to store max timing offset in

Returns: AriesErrorType - Aries error code

### `ariesMarginReportMaxVoltageOffset`

```c
AriesErrorType ariesMarginReportMaxVoltageOffset(AriesRxMarginType* marginDevice, int* maxVoltageOffset);
```

Margin Command to report the max voltage offset

- `marginDevice`: Struct containing Margin Device information
- `maxVoltageOffset`: variable to store max voltage offset in

Returns: AriesErrorType - Aries error code

### `ariesMarginReportSamplingRateVoltage`

```c
AriesErrorType ariesMarginReportSamplingRateVoltage(AriesRxMarginType* marginDevice, int* samplingRateVoltage);
```

Margin Command to report sampling rate voltage

- `marginDevice`: Struct containing Margin Device information
- `samplingRateVoltage`: variable to store samplingRateVoltage in

Returns: AriesErrorType - Aries error code

### `ariesMarginReportSamplingRateTiming`

```c
AriesErrorType ariesMarginReportSamplingRateTiming(AriesRxMarginType* marginDevice, int* samplingRateTiming);
```

Margin Command to report sampling rate timing

- `marginDevice`: Struct containing Margin Device information
- `samplingRateTiming`: variable to store samplingRateTiming in;

Returns: AriesErrorType - Aries error code

### `ariesMarginReportSampleCount`

```c
AriesErrorType ariesMarginReportSampleCount(AriesRxMarginType* marginDevice);
```

Margin Command to report sample count

- `marginDevice`: Struct containing Margin Device information

Returns: AriesErrorType - Aries error code

### `ariesMarginReportMaxLanes`

```c
AriesErrorType ariesMarginReportMaxLanes(AriesRxMarginType* marginDevice, int* maxLanes);
```

Margin Command to report the max lanes

- `marginDevice`: Struct containing Margin Device information
- `maxLanes`: variable to store maxLanes in

Returns: AriesErrorType - Aries error code

### `ariesMarginSetErrorCountLimit`

```c
AriesErrorType ariesMarginSetErrorCountLimit(AriesRxMarginType* marginDevice, int limit);
```

Margin Command to set the ErrorCountLimit

- `marginDevice`: Struct containing Margin Device information
- `limit`: New ErrorCountLimit

Returns: AriesErrorType - Aries error code

### `ariesMarginGoToNormalSettings`

```c
AriesErrorType ariesMarginGoToNormalSettings(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int laneCount);
```

Margin Command to go to normal settings

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `laneCount`: length of physical lane list

Returns: AriesErrorType - Aries error code

### `ariesMarginClearErrorLog`

```c
AriesErrorType ariesMarginClearErrorLog(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int laneCount);
```

Margin Command to clears error count of a given port and lane

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `laneCount`: length of physical lane list

Returns: AriesErrorType - Aries error code

### `ariesMarginStepMarginToTimingOffset`

```c
AriesErrorType ariesMarginStepMarginToTimingOffset( AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int* direction, int* steps, int laneCount, float dwell);
```

Margin Command to Sets the margin sampler to a specific time offset

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `direction`: Direction to move the sampler (0: left, 1: right)
- `steps`: Number of steps to move the sampler
- `laneCount`: length of physical lane list
- `dwell`: Time between starting sampling and reading sampling

Returns: AriesErrorType - Aries error code

### `ariesMarginStepMarginToVoltageOffset`

```c
AriesErrorType ariesMarginStepMarginToVoltageOffset( AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int* direction, int* steps, int laneCount, float dwell);
```

Margin Command to Sets the margin sampler to a specific voltage offset

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `direction`: Direction to move the sampler (0: up, 1: down)
- `steps`: Number of steps to move the sampler
- `laneCount`: length of physical lane list
- `dwell`: Time between starting sampling and reading sampling

Returns: AriesErrorType - Aries error code

### `ariesMarginVendorDefined`

```c
AriesErrorType ariesMarginVendorDefined(AriesRxMarginType* marginDevice);
```

Margin Command for vendor defined Vendor defined function is not implemented yet

- `marginDevice`: Struct containing Margin Device information

Returns: AriesErrorType - Aries error code

### `ariesMarginPmaRxMarginStop`

```c
AriesErrorType ariesMarginPmaRxMarginStop(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int laneCount);
```

Stops the margining

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `laneCount`: length of physical lane list

Returns: AriesErrorType - Aries error code

### `ariesMarginPmaRxMarginTiming`

```c
AriesErrorType ariesMarginPmaRxMarginTiming(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int* direction, int* steps, int laneCount);
```

Sets the PMA registers to margin a specified time value

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `direction`: Direction to move (0:left, 1:right)
- `steps`: Number of steps to move
- `laneCount`: length of physical lane list

Returns: AriesErrorType - Aries error code

### `ariesMarginPmaRxMarginVoltage`

```c
AriesErrorType ariesMarginPmaRxMarginVoltage(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int* direction, int* steps, int laneCount);
```

Sets the PMA registers to margin a specified voltage value

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `direction`: Direction to move (0:up, 1:down)
- `steps`: Number of steps to move
- `laneCount`: length of physical lane list

Returns: AriesErrorType - Aries error code

### `ariesMarginPmaTimingVoltageOffset`

```c
AriesErrorType ariesMarginPmaTimingVoltageOffset( AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int* timeDirection, int* timeSteps, int* voltageDirection, int* voltageSteps, int laneCount, float dwell);
```

Sets the PMA registers to margin a specified voltage and time value

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `timeDirection`: Direction to move on time axis (0: left, 1: right)
- `timeSteps`: Number of steps to move on time axis
- `voltageDirection`: Direction to move on voltage axis (0: up, 1: down)
- `voltageSteps`: Number of steps to move on the voltage axis
- `laneCount`: length of physical lane list
- `dwell`: Time to wait between starting margining and reading error count

Returns: AriesErrorType - Aries error code

### `ariesMarginPmaRxMarginGetECount`

```c
AriesErrorType ariesMarginPmaRxMarginGetECount(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int laneCount);
```

Gets the number of errors that have occurred on a specific port and lane

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `laneCount`: length of physical lane list

Returns: AriesErrorType - Aries Error Code

### `ariesMarginPmaRxReqAckHandshake`

```c
AriesErrorType ariesMarginPmaRxReqAckHandshake(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int laneCount);
```

Perform Rx Request/Ack Handshake

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `laneCount`: length of physical lane list

Returns: AriesErrorType - Aries error code

### `ariesMarginDeterminePmaSideAndQs`

```c
AriesErrorType ariesMarginDeterminePmaSideAndQs(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int lane, int* side, int* quadSlice, int* quadSliceLane);
```

Determines the pma side and quadslice of a port and lane

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Physical device lane on the Retimer
- `side`: Side the port and lane are in
- `quadSlice`: Quad slice the port and lane are in
- `quadSliceLane`: Quad slice lane the port and lane are in

Returns: AriesErrorType - Aries error code

### `ariesGetLaneRecoveryCount`

```c
AriesErrorType ariesGetLaneRecoveryCount(AriesRxMarginType* marginDevice, int* lane, int laneCount, uint8_t* recoveryCount);
```

Gets the recovery count of a lane

- `marginDevice`: Struct containing Margin Device information
- `lane`: Pointer to list of physical device lanes on the Retimer
- `laneCount`: Length of physical lane list
- `recoveryCount`: Variable to store the recovery count in

### `ariesClearLaneRecoveryCount`

```c
AriesErrorType ariesClearLaneRecoveryCount(AriesRxMarginType* marginDevice, int* lane, int laneCount);
```

Clears the recovery count of a lane

- `marginDevice`: Struct containing Margin Device information
- `lane`: Pointer to list of physical device lanes on the Retimer
- `laneCount`: Length of physical lane list

### `ariesCheckEye`

```c
AriesErrorType ariesCheckEye(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int* lane, int laneCount, float dwell);
```

Determines eye stats for a given port and lane using binary search

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `lane`: Pointer to list of physical device lanes on the Retimer
- `laneCount`: length of physical lane list
- `dwell`: Time to wait before checking error count in a lane

Returns: AriesErrorType - Aries error code

### `ariesLogEye`

```c
AriesErrorType ariesLogEye(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int width, const char* filename, int startLane, float dwell);
```

Calculates the eye for each lane on the device's port in parallel and outputs it to a file Calculates the eye for each lane in range start_lane - start_lane+width, and outputs timing UI, voltage UI, and additional debug info to a .csv.

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `width`: Width of the device (x16 or x8)
- `filename`: Name of the file for the data to be stored in
- `startLane`: Lane to start at on the Retimer
- `dwell`: Time to wait before checking error count

Returns: AriesErrorType - Aries error code

### `ariesLogEyeSerial`

```c
AriesErrorType ariesLogEyeSerial(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int width, const char* filename, int startLane, float dwell);
```

Calculates the eye for each lane on the device's port one at a time and outputs it to a file Calculates the eye for each lane in range start_lane - start_lane+width, and outputs timing UI, voltage UI, and additional debug info to a .csv. Slower and less optimal than ariesLogEye due to checking lanes one at a time rather than in parallel, but useful to debug lanes one at a time.

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer (USPP or DSPP)
- `width`: Width of the device (x16 or x8)
- `filename`: Name of the file for the data to be stored in
- `startLane`: Lane to start at on the Retimer
- `dwell`: Time to wait before checking error count

Returns: AriesErrorType - Aries error code

### `ariesEyeDiagram`

```c
AriesErrorType ariesEyeDiagram(AriesRxMarginType* marginDevice, AriesPseudoPortType port, int lane, int rate, float dwell);
```

Creates a full 2D eye diagram for a given port and lane

- `marginDevice`: Struct containing Margin Device information
- `port`: Port to margin on the Retimer(USPP or DSPP)
- `lane`: Physical device lane on the Retimer
- `rate`: data rate of the Retimer (Gen3: 3, Gen4: 4, Gen5: 5)
- `dwell`: Time to wait before checking error count

Returns: AriesErrorType - Aries error code

## Per-lane diagnostics and helpers (`aries_misc.h`)

### `ariesReadFwVersion`

```c
AriesErrorType ariesReadFwVersion(AriesI2CDriverType* i2cDriver, int offset, uint8_t* dataVal);
```

Read FW version info from the Main Micro space

- `i2cDriver`: Aries i2c driver
- `offset`: Offset inside FW struct for member
- `dataVal`: - data captured from registeres

Returns: AriesErrorType - Aries error code

### `ariesI2CMasterInit`

```c
AriesErrorType ariesI2CMasterInit(AriesI2CDriverType* i2cDriver);
```

I2C Master init for EEPROM Write-Thru

### `ariesI2CMasterWriteCtrlReg`

```c
AriesErrorType ariesI2CMasterWriteCtrlReg(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t lengthDataBytes, uint8_t* values);
```

Write to I2C Master ctrl register

### `ariesI2CMasterSetFrequency`

```c
AriesErrorType ariesI2CMasterSetFrequency(AriesI2CDriverType* i2cDriver, int frequencyHz);
```

Set I2C Master frequency

- `i2cDriver`: Aries i2c driver
- `frequencyHz`: I2C Master frequency in Hz

Returns: AriesErrorType - Aries error code

### `ariesGetEEPROMImageEnd`

```c
int ariesGetEEPROMImageEnd(uint8_t* data);
```

Get the location where the EEPROM image ends, which is after a specific sequence - a55aa555aff. This can be used to help reduce the overall fw load time

- `data`: Full EEPROM image as a byte array
- `numBytes`: Number of bytes to write (<= 256k)

Returns: location - Index until which we have to write

### `ariesI2CMasterMultiBlockWrite`

```c
AriesErrorType ariesI2CMasterMultiBlockWrite(AriesDeviceType* device, uint16_t address, int numBytes, uint8_t* values);
```

Write multiple blocks of data to the EEPROM with help from Main Micro

- `device`: Aries Device struct
- `address`: EEPROM address
- `numBytes`: Number of bytes to write (<= 256k)
- `values`: EEPROM data to be written as a byte array

Returns: AriesErrorType - Aries error code

### `ariesI2CMasterRewriteAndVerifyByte`

```c
AriesErrorType ariesI2CMasterRewriteAndVerifyByte(AriesI2CDriverType* i2cDriver, int address, uint8_t* value);
```

Rewrite a byte of data and verify it got written to the EEPROM with help from Main Micro

- `i2cDriver`: Aries i2c driver
- `address`: EEPROM address to rewrite and verify
- `value`: Data to write to EEPROM (1 byte)

Returns: AriesErrorType - Aries error code

### `ariesI2CMasterSendAddress`

```c
AriesErrorType ariesI2CMasterSendAddress(AriesI2CDriverType* i2cDriver, int address);
```

Uses I2C master to set the address pointer in the EEPROM

- `i2cDriver`: Aries i2c driver
- `address`: EEPROM address to send

Returns: AriesErrorType - Aries error code

### `ariesI2CMasterSendByteBlockData`

```c
AriesErrorType ariesI2CMasterSendByteBlockData(AriesI2CDriverType* i2cDriver, int address, int numBytes, uint8_t* value);
```

Uses I2C master to write multiple bytes to EEPROM

- `i2cDriver`: Aries i2c driver
- `address`: EEPROM address to send
- `numBytes`: Number of bytes to be sent
- `value`: Data bytes to send

Returns: AriesErrorType - Aries error code

### `ariesI2CMasterSendByte`

```c
AriesErrorType ariesI2CMasterSendByte(AriesI2CDriverType* i2cDriver, uint8_t* value, int flag);
```

Uses I2C master to write a byte to EEPROM

- `i2cDriver`: Aries i2c driver
- `value`: Data byte
- `flag`: I2C flag

Returns: AriesErrorType - Aries error code

### `ariesI2CMasterReceiveByteBlock`

```c
AriesErrorType ariesI2CMasterReceiveByteBlock(AriesDeviceType* device, uint8_t* dataBytes);
```

Uses I2C master to read multiple bytes to EEPROM

- `device`: Aries Device struct
- `dataBytes`: Data bytes recieved

Returns: AriesErrorType - Aries error code

### `ariesI2CMasterGetChecksum`

```c
AriesErrorType ariesI2CMasterGetChecksum(AriesDeviceType* device, uint16_t blockEnd, uint32_t* checksum);
```

Calculate checksum of this EEPROM block

- `i2cDriver`: Aries i2c driver
- `checksum`: Checksum value returned by function

Returns: AriesErrorType - Aries error code

### `ariesGetTempCalibrationCodes`

```c
AriesErrorType ariesGetTempCalibrationCodes(AriesDeviceType* device);
```

Get temp calibration codes, lot ID, and chip ID from eFuse.

- `device`: Aries Device struct

Returns: AriesErrorType - Aries error code

### `ariesReadPmaTempMax`

```c
AriesErrorType ariesReadPmaTempMax(AriesDeviceType* device);
```

Get max. temp reading across all PMAs

- `device`: Aries Device struct

Returns: AriesErrorType - Aries error code

### `ariesReadPmaAvgTemp`

```c
AriesErrorType ariesReadPmaAvgTemp(AriesDeviceType* device);
```

Get current avg. temp reading across all PMAs

- `device`: Aries Device struct

Returns: AriesErrorType - Aries error code

### `ariesEnableThermalShutdown`

```c
AriesErrorType ariesEnableThermalShutdown(AriesDeviceType* device);
```

Enable thermal shutdown in Aries

- `device`: Aries Device struct

Returns: AriesErrorType - Aries error code

### `ariesDisableThermalShutdown`

```c
AriesErrorType ariesDisableThermalShutdown(AriesDeviceType* device);
```

Disable thermal shutdown in Aries

- `device`: Aries Device struct

Returns: AriesErrorType - Aries error code

### `ariesReadPmaTemp`

```c
AriesErrorType ariesReadPmaTemp(AriesDeviceType* device, int side, int qs, float* temperature_C);
```

Get PMA Temp reading

- `device`: Aries Device struct
- `side`: PMA side
- `qs`: PMA Quad Slice
- `temperature_C`: PMA Temperature reading

Returns: AriesErrorType - Aries error code

### `ariesGetPortOrientation`

```c
AriesErrorType ariesGetPortOrientation(AriesDeviceType* device, int* orientation);
```

Get port orientation

- `i2cDriver`: i2c Driver struct
- `orientation`: port orientation

Returns: AriesErrorType - Aries error code

### `ariesGetPmaNumber`

```c
int ariesGetPmaNumber(int absLane);
```

Get PMA ID

- `absLane`: Absolute lane number

Returns: int - pma id.

### `ariesGetPmaLane`

```c
int ariesGetPmaLane(int absLane);
```

Get PMA lane ID

- `absLane`: Absolute lane number

Returns: int - pma lane id.

### `ariesGetPathID`

```c
int ariesGetPathID(int lane, int direction);
```

Get absolute Path ID

- `lane`: start lane of link
- `direction`: upstream/downsteram direction

Returns: int - path id.

### `ariesGetPathLaneID`

```c
int ariesGetPathLaneID(int lane);
```

Get Path lane ID

- `lane`: start lane of link

Returns: int - path lane id.

### `ariesDumpPMARegs`

```c
AriesErrorType ariesDumpPMARegs(AriesDeviceType* device, uint16_t* pma_addr, int len, char* filename);
```

Dump PMA registers to a file

- `device`: Aries Device struct
- `pma_addr`: Array of uint16_t register addresses to log
- `len`: Length of pma_addr array
- `filename`: File to write register dump to

Returns: AriesErrorType - Aries error code

### `ariesGetLinkRxTerm`

```c
AriesErrorType ariesGetLinkRxTerm(AriesLinkType* link, int side, int absLane, int* term);
```

Get link termination status

- `link`: Link struct created by user
- `side`: pma side - a (0) or b (1)
- `abslane`: link lane number
- `term`: term value returned

Returns: AriesErrorType - Aries error code

### `ariesGetLogicalLaneNum`

```c
AriesErrorType ariesGetLogicalLaneNum(AriesLinkType* link, int lane, int direction, int* laneNum);
```

Get Logical Lane Num

- `link`: Link struct created by user
- `lane`: link lane number
- `direction`: link direction
- `laneNum`: logical lane number

Returns: AriesErrorType - Aries error code

### `ariesGetTxPre`

```c
AriesErrorType ariesGetTxPre(AriesLinkType* link, int lane, int direction, int* txPre);
```

Get TX pre cursor value

- `link`: Link struct created by user
- `lane`: link lane number
- `direction`: link direction
- `txPre`: tx pre cursor value

Returns: AriesErrorType - Aries error code

### `ariesGetTxCur`

```c
AriesErrorType ariesGetTxCur(AriesLinkType* link, int lane, int direction, int* txCur);
```

Get TX current cursor value

- `link`: Link struct created by user
- `lane`: link lane number
- `direction`: link direction
- `txCur`: tx current cursor value

Returns: AriesErrorType - Aries error code

### `ariesGetTxPst`

```c
AriesErrorType ariesGetTxPst(AriesLinkType* link, int lane, int direction, int* txPst);
```

Get TX port cursor value

- `link`: Link struct created by user
- `lane`: link lane number
- `direction`: link direction
- `txPst`: tx pst value

Returns: AriesErrorType - Aries error code

### `ariesGetRxPolarityCode`

```c
AriesErrorType ariesGetRxPolarityCode(AriesLinkType* link, int lane, int direction, int pinSet, int* polarity);
```

Get RX polarity code

- `link`: Link struct created by user
- `lane`: link lane number
- `direction`: link direction
- `pinSet`: location in pins array
- `polarity`: polarity code

Returns: AriesErrorType - Aries error code

### `ariesGetRxAttCode`

```c
AriesErrorType ariesGetRxAttCode(AriesLinkType* link, int side, int absLane, int* code);
```

Get current RX att code

- `link`: Link struct created by user
- `side`: pma side - a (0) or b (1)
- `absLane`: Absolute lane number
- `code`: rx att code

Returns: AriesErrorType - Aries error code

### `ariesGetRxCtleBoostCode`

```c
AriesErrorType ariesGetRxCtleBoostCode(AriesLinkType* link, int side, int absLane, int* boostCode);
```

Get current RX VGA Code

- `link`: Link struct created by user
- `side`: pma side - a (0) or b (1)
- `absLane`: Absolute lane number
- `boostCode`: RX boost code

Returns: AriesErrorType - Aries error code

### `ariesGetRxVgaCode`

```c
AriesErrorType ariesGetRxVgaCode(AriesLinkType* link, int side, int absLane, int* vgaCode);
```

Get current RX VGA Code

- `link`: Link struct created by user
- `side`: pma side - a (0) or b (1)
- `absLane`: Absolute lane number
- `vgaCode`: RX VGA code

Returns: AriesErrorType - Aries error code

### `ariesGetRxBoostValueDb`

```c
float ariesGetRxBoostValueDb(int boostCode, float attValDb, int vgaCode);
```

Get current RX Boost Value (in dB)

- `boostCode`: boostCode
- `attValDb`: att val (in db)
- `vgaCode`: vga code

Returns: float - rx Boost value

### `ariesGetRxCtlePoleCode`

```c
AriesErrorType ariesGetRxCtlePoleCode(AriesLinkType* link, int side, int absLane, int* poleCode);
```

Get the current RX CTLE POLE code

- `link`: Link struct created by user
- `side`: PMA side
- `absLane`: Absolute lane number
- `poleCode`: POLE code captured

Returns: AriesErrorType - Aries error code

### `ariesGetRxAdaptIq`

```c
AriesErrorType ariesGetRxAdaptIq(AriesLinkType* link, int side, int absLane, int* iqValue);
```

Get the current RX Adapt IQ Value

- `link`: Link struct created by user
- `side`: PMA side
- `absLane`: Absolute lane number
- `iqValue`: Adapt IQ value captured

Returns: AriesErrorType - Aries error code

### `ariesGetRxAdaptIqBank`

```c
AriesErrorType ariesGetRxAdaptIqBank(AriesLinkType* link, int side, int absLane, int bank, int* iqValue);
```

Get the current RX Adapt IQ Value

- `link`: Link struct created by user
- `side`: PMA side
- `absLane`: Absolute lane number
- `bank`: Bank number
- `iqValue`: Adapt IQ value captured

Returns: AriesErrorType - Aries error code

### `ariesGetRxAdaptDoneBank`

```c
AriesErrorType ariesGetRxAdaptDoneBank(AriesLinkType* link, int side, int absLane, int bank, int* doneValue);
```

Get the current RX Adapt IQ Value

- `link`: Link struct created by user
- `side`: PMA side
- `absLane`: Absolute lane number
- `bank`: Bank number
- `doneValue`: Adapt done value captured

Returns: AriesErrorType - Aries error code

### `ariesGetRxDfeCode`

```c
AriesErrorType ariesGetRxDfeCode(AriesLinkType* link, int side, int absLane, int tapNum, int* dfeCode);
```

Get the current RX DFE code

- `link`: Link struct created by user
- `side`: PMA side
- `absLane`: Absolute lane number
- `tapNum`: DFE tap code
- `dfeCode`: dfe code captured

Returns: AriesErrorType - Aries error code

### `ariesGetLastEqSpeed`

```c
AriesErrorType ariesGetLastEqSpeed(AriesLinkType* link, int lane, int direction, int* speed);
```

Get the last speed at which EQ was run (e.g. 3 for Gen-3)

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `direction`: PMA side
- `speed`: last captures speed

Returns: AriesErrorType - Aries error code

### `ariesGetDeskewStatus`

```c
AriesErrorType ariesGetDeskewStatus(AriesLinkType* link, int lane, int direction, int* status);
```

Get the deskew status string for the given lane

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `direction`: PMA side
- `val`: deskew status for lane

Returns: AriesErrorType - Aries error code

### `ariesGetDeskewClks`

```c
AriesErrorType ariesGetDeskewClks(AriesLinkType* link, int lane, int direction, int* val);
```

Get the number of clocks of deskew applied for the given lane

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `direction`: PMA side
- `val`: deskew clocks for lane

Returns: AriesErrorType - Aries error code

### `ariesGetLastEqReqPre`

```c
AriesErrorType ariesGetLastEqReqPre(AriesLinkType* link, int lane, int direction, int* val);
```

For the last round of Equalization, get the final pre-cursor request the link partner made. If the last request was a Perest request, this value will be 0.

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `direction`: PMA side
- `val`: Pre-cursor request value

Returns: AriesErrorType - Aries error code

### `ariesGetLastEqReqCur`

```c
AriesErrorType ariesGetLastEqReqCur(AriesLinkType* link, int lane, int direction, int* val);
```

For the last round of Equalization, get the final cursor request the link partner made. If the last request was a Perest request, this value will be 0.

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `direction`: PMA side
- `val`: cursor request value

Returns: AriesErrorType - Aries error code

### `ariesGetLastEqReqPst`

```c
AriesErrorType ariesGetLastEqReqPst(AriesLinkType* link, int lane, int direction, int* val);
```

For the last round of Equalization, get the final post-cursor request the link partner made. If the last request was a Perest request, this value will be 0.

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `direction`: PMA side
- `val`: Post-cursor request value

Returns: AriesErrorType - Aries error code

### `ariesGetLastEqReqPreset`

```c
AriesErrorType ariesGetLastEqReqPreset(AriesLinkType* link, int lane, int direction, int* val);
```

For the last round of Equalization, get the final preset request the link partner made. A value of 0xf will indicate the final request was a coefficient request.

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `direction`: PMA side
- `val`: Preset request value

Returns: AriesErrorType - Aries error code

### `ariesGetLastEqPresetReq`

```c
AriesErrorType ariesGetLastEqPresetReq(AriesLinkType* link, int lane, int direction, int reqNum, int* val);
```

For the last round of Equalization, get the Preset value for the specified request.

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `reqNum`: Request num
- `val`: Preset value

Returns: AriesErrorType - Aries error code

### `ariesGetLastEqPresetReqFOM`

```c
AriesErrorType ariesGetLastEqPresetReqFOM(AriesLinkType* link, int lane, int direction, int reqNum, int* val);
```

For the last round of Equalization, get the FOM value for the specified request.

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `reqNum`: Request num
- `val`: FOM value

Returns: AriesErrorType - Aries error code

### `ariesGetLastEqNumPresetReq`

```c
AriesErrorType ariesGetLastEqNumPresetReq(AriesLinkType* link, int lane, int direction, int* val);
```

For the last round of Equalization, get the number of reset requests issued.

- `link`: Link struct created by user
- `lane`: Absolute lane number
- `reqNum`: Request num
- `val`: Number of reset requests

Returns: AriesErrorType - Aries error code

### `ariesGetLoggerFmtID`

```c
AriesErrorType ariesGetLoggerFmtID(AriesLinkType* link, AriesLTSSMLoggerEnumType loggerType, int offset, int* fmtID);
```

Get the format ID at offset in the print buffer

- `link`: Link struct created by user
- `loggerType`: Logger type (main or which path)
- `offset`: Print buffer offset
- `fmtID`: Format ID from logger

Returns: AriesErrorType - Aries error code

### `ariesGetLoggerWriteOffset`

```c
AriesErrorType ariesGetLoggerWriteOffset(AriesLinkType* link, AriesLTSSMLoggerEnumType loggerType, int* writeOffset);
```

Get the write pointer location in the LTSSM logger

- `link`: Link struct created by user
- `loggerType`: Logger type (main or which path)
- `writeOffset`: Current write offset

Returns: AriesErrorType - Aries error code

### `ariesGetLoggerOneBatchModeEn`

```c
AriesErrorType ariesGetLoggerOneBatchModeEn(AriesLinkType* link, AriesLTSSMLoggerEnumType loggerType, int* oneBatchModeEn);
```

Check if one batch mode in the LTSSM logger is enabled or not

- `link`: Link struct created by user
- `loggerType`: Logger type (main or which path)
- `oneBatchModeEn`: One batch mode enabled or not

Returns: AriesErrorType - Aries error code

### `ariesGetLoggerOneBatchWrEn`

```c
AriesErrorType ariesGetLoggerOneBatchWrEn(AriesLinkType* link, AriesLTSSMLoggerEnumType loggerType, int* oneBatchWrEn);
```

Check if one batch write in the LTSSM logger is enabled or not

- `link`: Link struct created by user
- `loggerType`: Logger type (main or which path)
- `oneBatchWrEn`: One batch write enabled or not

Returns: AriesErrorType - Aries error code

### `ariesGetPecByte`

```c
uint8_t ariesGetPecByte(uint8_t* polynomial, uint8_t length);
```

Calculate CRC-8 byte from polynomial

- `msg`: Polynomial to calculate PEC
- `length`: Length of polynomial

Returns: uint8_t - crc-8 byte

### `ariesGetMinFoMVal`

```c
AriesErrorType ariesGetMinFoMVal(AriesDeviceType* device, int side, int pathID, int lane, int regOffset, uint8_t* data);
```

Capture the min FoM value seen for a given lane.

- `device`: Device struct containing i2c driver
- `side`: PMA side
- `pathID`: Path ID
- `lane`: lane inside path
- `regOffset`: address offset for FoM reg
- `data`: 2-byte data returned, containing FoM value

Returns: AriesErrorType - Aries error code

### `ariesEepromReadBlockData`

```c
AriesErrorType ariesEepromReadBlockData(AriesDeviceType* device, uint8_t* values, int startAddr, uint8_t numBytes);
```

Read numBytes bytes of data starting at startAddr

### `ariesEepromCalcChecksum`

```c
AriesErrorType ariesEepromCalcChecksum(AriesDeviceType* device, int startAddr, int numBytes, uint8_t* checksum);
```

Read numBytes bytes from the EEPROM starting at startAddr and calculate a running checksum (e.g. add the bytes as you read them): uint8_t checksum = (checksum + new_byte) % 256

### `ariesSortArray`

```c
void ariesSortArray(uint16_t* arr, int size);
```

Sort Array in ascending order

- `arr`: Array to be sorted
- `size`: Num elements in array

Returns: None

### `ariesGetMedian`

```c
uint16_t ariesGetMedian(uint16_t* arr, int size);
```

Get median of all elements in the array

- `arr`: Array to be sorted
- `size`: Num elements in array

Returns: None

### `ariesLoadBinFile`

```c
AriesErrorType ariesLoadBinFile(const char* filename, uint8_t* mem);
```

This loads an binary file into the mem[] array

### `ariesLoadIhxFile`

```c
AriesErrorType ariesLoadIhxFile(const char* filename, uint8_t* mem);
```

This loads an intel hex file into the mem[] array

### `ariesParseIhxLine`

```c
AriesErrorType ariesParseIhxLine(char* line, uint8_t* bytes, int* addr, int* num, int* status);
```

This is used by loadFile to get each line of intex hex

### `ariesReadBlockDataForceError`

```c
AriesErrorType ariesReadBlockDataForceError(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Read multiple data bytes from Aries over I2C. Purposely force an error in the transaction by injecting an intervening I2C transaction in the middle between the Write and Read.

### `ariesTsenseADCToDegrees`

```c
float ariesTsenseADCToDegrees(AriesDeviceType* device, int adcCode, uint8_t calCode);
```

Convert the PMA ADC code to degrees Celsius

- `device`: Aries Device struct
- `adcCode`: Temperature ACD code from PMA
- `calCode`: PMA calibration code

Returns: temperature_C Temperature in Degrees Celsius

### Declared without a doc comment

- `ariesEEPROMGetBlockLength`
- `ariesEEPROMGetRandomByte`
- `ariesFirmwareIsAtLeast`
- `ariesGetCrcBytesImage`
- `ariesGetEEPROMBlockCrcByte`
- `ariesGetEEPROMBlockType`
- `ariesGetEEPROMFirstBlock`
- `ariesGetLaneNum`
- `ariesGetLinkCurrentSpeed`
- `ariesGetLinkId`
- `ariesGetPathFWState`
- `ariesGetPathHWState`
- `ariesGetPinMap`
- `ariesGetQSPathInfo`
- `ariesGetStartLane`
- `ariesI2CMasterReceiveByte`
- `ariesI2CMasterReceiveContinuousByte`
- `ariesI2CMasterSetPage`
- `ariesI2cMasterSoftReset`
- `ariesPMABertPatChkConfig`
- `ariesPMABertPatChkDetectCorrectPolarity`
- `ariesPMABertPatChkSts`
- `ariesPMABertPatChkToggleSync`
- `ariesPMABertPatGenConfig`
- `ariesPMAPCSRxRecalBankOvrdSet`
- `ariesPMAPCSRxReqBlock`
- `ariesPMARxDataEnSet`
- `ariesPMARxInvertSet`
- `ariesPMATxDataEnSet`
- `ariesPMAVregVrefSet`
- `ariesPipeBlkAlgnCtrlSet`
- `ariesPipeDeepmhasisSet`
- `ariesPipeFomGet`
- `ariesPipePhyStatusClear`
- `ariesPipePhyStatusGet`
- `ariesPipePhyStatusToggle`
- `ariesPipePowerdownCheck`
- `ariesPipePowerdownSet`
- `ariesPipeRateChange`
- `ariesPipeRateCheck`
- `ariesPipeRxAdapt`
- `ariesPipeRxEqEval`
- `ariesPipeRxEqInProgressSet`
- `ariesPipeRxPolaritySet`
- `ariesPipeRxStandbySet`
- `ariesPipeRxTermSet`
- `ariesPipeTxElecIdleSet`
- `ariesReadPmaAvgTempDirect`
- `ariesSetPortOrientation`

## I2C transport and raw register access (`aries_i2c.h`)

### `ariesRunArp`

```c
AriesErrorType ariesRunArp(int handle, uint8_t new7bitSmbusAddr);
```

Set Slave address to user-specified value: new7bitSmbusAddr

- `handle`: Handle to I2C driver
- `new7bitSmbusAddr`: Desired I2C (7-bit) address of Retimer

Returns: AriesErrorType - Aries error code

### `ariesWriteBlockData`

```c
AriesErrorType ariesWriteBlockData(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Write multiple bytes of data at specified address to Aries over I2C. This function returns a negative error code if unsuccessful, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 17-bit address to write
- `numBytes`: Number of bytes to write
- `values`: Pointer to array of data (bytes) you wish to write

Returns: AriesErrorType - Aries error code

### `ariesWriteByteData`

```c
AriesErrorType ariesWriteByteData(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t* value);
```

Write a data byte at specified address to Aries over I2C. This function returns a negative error code if unsuccessful, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 17-bit address to write
- `value`: Pointer to single element array of data you wish to write

Returns: AriesErrorType - Aries error code

### `ariesReadBlockData`

```c
AriesErrorType ariesReadBlockData(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Read multiple bytes of data at specified address from Aries over I2C. This function returns a negative error code if unsuccessful, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 17-bit address from which to read
- `numBytes`: Number of bytes to read
- `values`: Pointer to array of read data (bytes)

Returns: AriesErrorType - Aries error code

### `ariesReadByteData`

```c
AriesErrorType ariesReadByteData(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t* values);
```

Read a data byte at specified address from Aries over I2C. This function returns a negative error code if unsuccessful, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 17-bit address from which to read
- `values`: Pointer to single element array of read data (byte)

Returns: AriesErrorType - Aries error code

### `ariesWriteReadBlockData`

```c
AriesErrorType ariesWriteReadBlockData(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Read multiple data bytes to Aries over I2C using combined transaction format. This function returns a negative error code if unsuccessful, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 17-bit address to read
- `numBytes`: Number of bytes to read
- `values`: Pointer to array of data (bytes) you wish to read

Returns: AriesErrorType - Aries error code

### `ariesReadBlockDataMainMicroIndirectA0`

```c
AriesErrorType ariesReadBlockDataMainMicroIndirectA0( AriesI2CDriverType* i2cDriver, uint32_t microIndStructOffset, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Read multiple (up to eight) data bytes from micro SRAM over I2C for A0. Returns a negative error code, or zero on success

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `microIndStructOffset`: Micro Indirect Struct Offset
- `address`: 16-bit address from which to read
- `numBytes`: Number of bytes to read
- `values`: Pointer to single element array of read data

Returns: AriesErrorType - Aries error code

### `ariesReadBlockDataMainMicroIndirectMPW`

```c
AriesErrorType ariesReadBlockDataMainMicroIndirectMPW( AriesI2CDriverType* i2cDriver, uint32_t microIndStructOffset, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Read multiple (up to eight) data bytes from micro SRAM over I2C. Returns a negative error code, or zero on success

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `microIndStructOffset`: Micro Indirect Struct Offset
- `address`: 16-bit address from which to read
- `numBytes`: Number of bytes to read
- `values`: Pointer to single element array of read data

Returns: AriesErrorType - Aries error code

### `ariesWriteBlockDataMainMicroIndirectA0`

```c
AriesErrorType ariesWriteBlockDataMainMicroIndirectA0( AriesI2CDriverType* i2cDriver, uint32_t microIndStructOffset, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Write multiple data bytes to specified address from micro SRAM over I2C for A0. Returns a negative error code, or zero on success

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `microIndStructOffset`: Micro Indirect Struct Offset
- `address`: 16-bit address from which to write
- `numBytes`: Number of bytes to write
- `values`: Pointer to array of data (bytes) you wish to write

Returns: AriesErrorType - Aries error code

### `ariesWriteBlockDataMainMicroIndirectMPW`

```c
AriesErrorType ariesWriteBlockDataMainMicroIndirectMPW( AriesI2CDriverType* i2cDriver, uint32_t microIndStructOffset, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Write multiple (up to eight) data bytes to specified address from micro SRAM over I2C. Returns a negative error code, or zero on success

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `microIndStructOffset`: Micro Indirect Struct Offset
- `address`: 16-bit address from which to write
- `numBytes`: Number of bytes to write
- `values`: Pointer to array of data (bytes) you wish to write

Returns: AriesErrorType - Aries error code

### `ariesReadByteDataMainMicroIndirect`

```c
AriesErrorType ariesReadByteDataMainMicroIndirect(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t* value);
```

Read one byte of data from specified address from Main micro SRAM over I2C. Returns a negative error code, else the number byte data read.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 16-bit address from which to read
- `value`: Pointer to single element array of read data (one byte)

Returns: AriesErrorType - Aries error code

### `ariesReadBlockDataMainMicroIndirect`

```c
AriesErrorType ariesReadBlockDataMainMicroIndirect(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Read multiple (up to eight) data bytes from specified address from Main micro SRAM over I2C. Returns a negative error code, else zero on success

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 16-bit address from which to read
- `numBytes`: Number of bytes to read (maximum 8 bytes)
- `values`: Pointer to array storing up to 8 bytes of data

Returns: AriesErrorType - Aries error code

### `ariesWriteByteDataMainMicroIndirect`

```c
AriesErrorType ariesWriteByteDataMainMicroIndirect(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t* value);
```

Write a data byte at specified address to Main micro SRAM Aries over I2C. Returns a negative error code, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 16-bit address to write
- `value`: Byte data to write

Returns: AriesErrorType - Aries error code

### `ariesWriteBlockDataMainMicroIndirect`

```c
AriesErrorType ariesWriteBlockDataMainMicroIndirect(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Write multiple (up to eight) data bytes at specified address to Main micro SRAM over I2C. Returns a negative error code, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 16-bit address to write
- `numBytes`: Number of bytes to write (maximum 16 bytes)
- `values`: Byte array which will be written

Returns: AriesErrorType - Aries error code

### `ariesReadBlockDataPathMicroIndirect`

```c
AriesErrorType ariesReadBlockDataPathMicroIndirect(AriesI2CDriverType* i2cDriver, uint8_t pathID, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Read multiple (up to eight) data bytes at specified address from Path micro SRAM over I2C. Returns a negative error code, else the number of bytes read.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `pathID`: Path micro ID (e.g. 0, 1, ..., 15)
- `address`: 16-bit address from which to read
- `numBytes`: Number of bytes to read (maximum 16 bytes)
- `values`: Pointer to array storing up to 16 bytes of data

Returns: AriesErrorType - Aries error code

### `ariesReadByteDataPathMicroIndirect`

```c
AriesErrorType ariesReadByteDataPathMicroIndirect(AriesI2CDriverType* i2cDriver, uint8_t pathID, uint32_t address, uint8_t* value);
```

Read a data byte at specified address from Path micro SRAM over I2C. Returns a negative error code, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `pathID`: Path micro ID (e.g. 0, 1, ..., 15)
- `address`: 16-bit address from which to read
- `value`: Pointer to array of one byte of read data

Returns: AriesErrorType - Aries error code

### `ariesWriteBlockDataPathMicroIndirect`

```c
AriesErrorType ariesWriteBlockDataPathMicroIndirect(AriesI2CDriverType* i2cDriver, uint8_t pathID, uint32_t address, uint8_t numBytes, uint8_t* values);
```

Write multiple (up to eight) data bytes at specified address to Path micro SRAM over I2C. Returns a negative error code, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `pathID`: Path micro ID (e.g. 0, 1, ..., 15)
- `address`: 16-bit address to write
- `numBytes`: Number of bytes to write (maximum 16 bytes)
- `values`: Byte array which will be written

Returns: AriesErrorType - Aries error code

### `ariesWriteByteDataPathMicroIndirect`

```c
AriesErrorType ariesWriteByteDataPathMicroIndirect(AriesI2CDriverType* i2cDriver, uint8_t pathID, uint32_t address, uint8_t* value);
```

Write one byte of data byte at specified address to Path micro SRAM Aries over I2C. Returns a negative error code, else zero on success.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `pathID`: Path micro ID (e.g. 0, 1, ..., 15)
- `address`: 16-bit address to write
- `value`: Byte data to write

Returns: AriesErrorType - Aries error code

### `ariesReadWordPmaIndirect`

```c
AriesErrorType ariesReadWordPmaIndirect(AriesI2CDriverType* i2cDriver, int side, int quadSlice, uint16_t address, uint8_t* values);
```

Read 2 bytes of data from PMA register over I2C

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0) or A (1)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `address`: 16-bit address from which to read
- `values`: Byte array which will be written

Returns: AriesErrorType - Aries error code

### `ariesWriteWordPmaIndirect`

```c
AriesErrorType ariesWriteWordPmaIndirect(AriesI2CDriverType* i2cDriver, int side, int quadSlice, uint16_t address, uint8_t* values);
```

Write 2 bytes of data from PMA register over I2C

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0), A (1), or broadcast to both (2)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `address`: 16-bit address to write
- `values`: Byte array which will be written

Returns: AriesErrorType - Aries error code

### `ariesReadWordPmaLaneIndirect`

```c
AriesErrorType ariesReadWordPmaLaneIndirect(AriesI2CDriverType* i2cDriver, int side, int quadSlice, int lane, uint16_t regOffset, uint8_t* values);
```

Read 2 bytes of data from PMA lane register over I2C

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0) or A (1)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `lane`: PMA lane number
- `regOffset`: 16-bit ref offset from which to read
- `values`: Byte array to store read data

Returns: AriesErrorType - Aries error code

### `ariesWriteWordPmaLaneIndirect`

```c
AriesErrorType ariesWriteWordPmaLaneIndirect(AriesI2CDriverType* i2cDriver, int side, int quadSlice, int lane, uint16_t regOffset, uint8_t* values);
```

Write 2 bytes of data to PMA lane register over I2C

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0), A (1), or broadcast to both (2)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `lane`: PMA lane number
- `regOffset`: 16-bit ref offset from which to read
- `values`: Byte array which will be written

Returns: AriesErrorType - Aries error code

### `ariesReadWordPmaMainMicroIndirect`

```c
AriesErrorType ariesReadWordPmaMainMicroIndirect(AriesI2CDriverType* i2cDriver, int side, int quadSlice, uint16_t pmaAddr, uint8_t* data);
```

Read 2 bytes of data from PMA register over I2C using the 'main-micro-assisted' indirect method. This method is necessary during mission-mode (non-PRBS-test-mode) operation.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0) or A (1)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `pmaAddr`: 16-bit PMA reg offset from which to read
- `data`: Byte array which will be contain data read

Returns: AriesErrorType - Aries error code

### `ariesWriteWordPmaMainMicroIndirect`

```c
AriesErrorType ariesWriteWordPmaMainMicroIndirect(AriesI2CDriverType* i2cDriver, int side, int quadSlice, uint16_t pmaAddr, uint8_t* data);
```

Write 2 bytes of data to PMA register over I2C using the 'main-micro-assisted' indirect method. This method is necessary during mission-mode (non-PRBS-test-mode) operation.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0), A (1), or broadcast to both (2)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `pmaAddr`: 16-bit PMA reg offset from which to read
- `data`: Byte array which will be written

Returns: AriesErrorType - Aries error code

### `ariesReadWordPmaLaneMainMicroIndirect`

```c
AriesErrorType ariesReadWordPmaLaneMainMicroIndirect(AriesI2CDriverType* i2cDriver, int side, int quadSlice, int lane, uint16_t regOffset, uint8_t* values);
```

Read 2 bytes of data from PMA lane register over I2C using the 'main-micro-assisted' indirect method. This method is necessary during mission-mode (non-PRBS-test-mode) operation.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0) or A (1)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `lane`: PMA lane number
- `regOffset`: 16-bit PMA reg offset from which to read
- `values`: Byte array which will be contain data read

Returns: AriesErrorType - Aries error code

### `ariesWriteWordPmaLaneMainMicroIndirect`

```c
AriesErrorType ariesWriteWordPmaLaneMainMicroIndirect(AriesI2CDriverType* i2cDriver, int side, int quadSlice, int lane, uint16_t regOffset, uint8_t* values);
```

Write 2 bytes of data to PMA lane register over I2C using the 'main-micro-assisted' indirect method. This method is necessary during mission-mode (non-PRBS-test-mode) operation.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0), A (1), or broadcast to both (2)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `lane`: PMA lane number
- `regOffset`: 16-bit ref offset from which to read
- `values`: Byte array which will be written

Returns: AriesErrorType - Aries error code

### `ariesReadWriteWordPmaLaneMainMicroIndirect`

```c
AriesErrorType ariesReadWriteWordPmaLaneMainMicroIndirect( AriesI2CDriverType* i2cDriver, int side, int quadSlice, int quadSliceLane, uint16_t pmaAddr, int offset, uint16_t value, int width);
```

Read Modify Write 2 bytes of data to PMA lane register over I2C using the 'main-micro-assisted' indirect method. This method is necessary during mission-mode (non-PRBS-test-mode) operation.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0), A (1), or broadcast to both (2)
- `quadSlice`: PMA num: 0, 1, 2, or 3
- `quadSliceLane`: PMA lane number
- `pmaAddr`: Address you wish to write to
- `offset`: Offset from the start of the word you want to write in
- `value`: The value to write
- `width`: The width of the field you are writing to

Returns: AriesErrorType - Aries error code

### `ariesReadRetimerRegister`

```c
AriesErrorType ariesReadRetimerRegister(AriesI2CDriverType* i2cDriver, int side, int lane, uint16_t baseAddr, uint8_t numBytes, uint8_t* data);
```

Read N bytes of data from a Retimer (gbl, ln0, or ln1) CSR.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0), A (1)
- `lane`: Absolute lane number (15:0)
- `baseAddr`: 16-bit base address
- `numBytes`: Number of bytes to read
- `data`: Byte array which will be contain data read

Returns: AriesErrorType - Aries error code

### `ariesWriteRetimerRegister`

```c
AriesErrorType ariesWriteRetimerRegister(AriesI2CDriverType* i2cDriver, int side, int lane, uint16_t baseAddr, uint8_t numBytes, uint8_t* data);
```

Write N bytes of data to a Retimer (gbl, ln0, or ln1) CSR.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `side`: PMA Side B (0), A (1)
- `lane`: Absolute lane number (15:0)
- `baseAddr`: 16-bit base address
- `numBytes`: Number of bytes to write
- `data`: Byte array containing data to be written

Returns: AriesErrorType - Aries error code

### `ariesReadWideRegister`

```c
AriesErrorType ariesReadWideRegister(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t width, uint8_t* values);
```

Read the contents of an Aries wide register.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 16-bit base address
- `width`: Size of wide register in bytes
- `values`: Byte array containing data to be written

Returns: AriesErrorType - Aries error code

### `ariesWriteWideRegister`

```c
AriesErrorType ariesWriteWideRegister(AriesI2CDriverType* i2cDriver, uint32_t address, uint8_t width, uint8_t* values);
```

Write the contents of an Aries wide register.

- `i2cDriver`: I2C driver responsible for the transaction(s)
- `address`: 16-bit base address
- `width`: Size of wide register in bytes
- `values`: Byte array containing data to be written

Returns: AriesErrorType - Aries error code

### `ariesLock`

```c
AriesErrorType ariesLock(AriesI2CDriverType* i2cDriver);
```

Set lock on bus (Aries transaction)

- `i2cDriver`: I2C driver responsible for the transaction(s)

Returns: AriesErrorType - Aries error code

### `ariesUnlock`

```c
AriesErrorType ariesUnlock(AriesI2CDriverType* i2cDriver);
```

Unlock bus after lock (Aries transaction)

- `i2cDriver`: I2C driver responsible for the transaction(s)

Returns: AriesErrorType - Aries error code

### Declared without a doc comment

- `asteraI2CBlock`
- `asteraI2COpenConnection`
- `asteraI2CReadBlockData`
- `asteraI2CUnblock`
- `asteraI2CWriteBlockData`
- `asteraI2CWriteReadBlockData`
