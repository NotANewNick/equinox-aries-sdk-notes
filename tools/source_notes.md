# ariesFirmwareIsAtLeast
Return true (1) if `device->fwVersion` is at least the given version. Returns false (0) when the stored version is 0.0.0, so the version must already have been read.
- device: Aries Device struct
- major: Minimum major version
- minor: Minimum minor version
- build: Minimum build number
Returns: int - 1 if the firmware is new enough, otherwise 0

# ariesI2CMasterSetPage
Select the EEPROM page used by the chip's I2C master, by setting its target address to `0x50 | (page & 3)`. The master is disabled during the change.
- i2cDriver: I2C driver responsible for the transaction(s)
- page: Page number; only the low 2 bits are used
Returns: AriesErrorType - Aries error code

# ariesI2CMasterReceiveByte
Read one byte from the EEPROM through the chip's I2C master, at the current address, and wait `ARIES_I2C_MASTER_CMD_RST` (2000 us).
- i2cDriver: I2C driver responsible for the transaction(s)
- value: Byte read
Returns: AriesErrorType - Aries error code

# ariesI2CMasterReceiveContinuousByte
Read the next byte in a stream without the setup or delay of `ariesI2CMasterReceiveByte`. The SDK uses this for the bytes after the first when verifying an EEPROM image.
- i2cDriver: I2C driver responsible for the transaction(s)
- value: Byte read
Returns: AriesErrorType - Aries error code

# ariesI2cMasterSoftReset
Recover the bus between the chip and the EEPROM: enable bit-bang mode, send a start, 9 clocks and a stop, then disable bit-bang mode.
- i2cDriver: I2C driver responsible for the transaction(s)
Returns: AriesErrorType - Aries error code

# ariesReadPmaAvgTempDirect
Read all 8 PMA temperature sensors directly and store their average in `device->currentTempC`. This writes PMA registers, so it is not a passive read. If a sensor returns an invalid code, it returns `ARIES_TEMP_READ_NOT_READY` immediately, without running that sensor's disable steps.
- device: Aries Device struct; its PMA calibration codes are used
Returns: AriesErrorType - Aries error code

# ariesGetQSPathInfo
Convert a lane and direction into a quad slice, path and path lane number.
- lane: Absolute lane number
- direction: Direction, added to the even path base
- qs: Quad slice, path ID / 4
- qsPath: Path within the quad slice, path ID % 4
- qsPathLane: Lane within the path, lane % 2

# ariesGetStartLane
Return the link's start lane, adding 4 on `ARIES_PTX08` parts.
- link: Link struct created by user
Returns: int - start lane

# ariesGetLinkId
Find which link in a bifurcation mode contains a lane. Returns `ARIES_FAILURE` if the lane is beyond the mode's links.
- bifMode: Bifurcation mode
- lane: Lane number, counted from the start of the bifurcation mode
- linkNum: Link index
Returns: AriesErrorType - Aries error code

# ariesGetLinkCurrentSpeed
Read a path's current PCIe rate as 2.5, 5, 8, 16 or 32, the per-lane transfer rates for Gen1 to Gen5. Other register values give 0.
- link: Link struct created by user
- lane: Absolute lane number
- direction: Direction
- speed: Current speed
Returns: AriesErrorType - Aries error code

# ariesGetLaneNum
Declared in `aries_misc.h`, but the SDK has no definition, so calling it fails at link time. Use `ariesGetLogicalLaneNum` for logical lane numbers.
- link: Link struct created by user
- lane: Lane number
- laneNum: Output lane number
Returns: AriesErrorType - Aries error code

# ariesGetPinMap
Fill `device->pins` with the chip's PCIe lane signal names, such as `B_PER0` and `A_PET0`, and package inversion flags. This does not describe the card's debug headers. Returns `ARIES_INVALID_ARGUMENT` for parts other than `ARIES_PTX16` and `ARIES_PTX08`.
- device: Aries Device struct; `partNumber` must be set
Returns: AriesErrorType - Aries error code

# ariesGetCrcBytesImage
Walk a firmware image held in memory and collect each block's CRC byte. It stops at block type 0xff or after `ARIES_EEPROM_MAX_NUM_CRC_BLOCKS` (10) blocks.
- image: Firmware image
- crcBytes: Output buffer, with room for at least 10 bytes
- numCrcBytes: Number of CRC bytes found

# ariesEEPROMGetBlockLength
Read a block's 16-bit length from `blockStartAddr + 5` (LSB) and `+ 6` (MSB).
- i2cDriver: I2C driver responsible for the transaction(s)
- blockStartAddr: EEPROM address of the block
- blockLength: Block length
Returns: AriesErrorType - Aries error code

# ariesEEPROMGetRandomByte
Read one EEPROM byte at any address through the chip's I2C master. The page is `addr / 65536`.
- i2cDriver: I2C driver responsible for the transaction(s)
- addr: EEPROM address
- value: Byte read
Returns: AriesErrorType - Aries error code

# ariesGetEEPROMBlockCrcByte
Read a block's stored CRC byte from `blockStartAddr + blockLength + 11`.
- i2cDriver: I2C driver responsible for the transaction(s)
- blockStartAddr: EEPROM address of the block
- blockLength: Block length
- crcByte: Stored CRC byte
Returns: AriesErrorType - Aries error code

# ariesGetEEPROMBlockType
Read a block's type byte from `blockStartAddr + 4`.
- i2cDriver: I2C driver responsible for the transaction(s)
- blockStartAddr: EEPROM address of the block
- blockType: Block type
Returns: AriesErrorType - Aries error code

# ariesGetEEPROMFirstBlock
Search the start of the EEPROM for the first block marker, `a5 5a a5 5a`, stopping after 50 search steps. If no marker is found it still returns success and leaves `*blockStartAddr` unchanged, so set a sentinel value first.
- i2cDriver: I2C driver responsible for the transaction(s)
- blockStartAddr: Address of the first block
Returns: AriesErrorType - Aries error code

# ariesGetPathFWState
Read a path's firmware state from the path microcontroller.
- link: Link struct created by user
- lane: Absolute lane number
- direction: Direction
- state: Firmware state value
Returns: AriesErrorType - Aries error code

# ariesGetPathHWState
Read a path's hardware state from its `ARIES_RET_PTH_NEXT_STATE_OFFSET` register.
- link: Link struct created by user
- lane: Absolute lane number
- direction: Direction
- state: Hardware state value
Returns: AriesErrorType - Aries error code

# ariesSetPortOrientation
Set the orientation bit, bit 8 of wide register 0x10, with a read-modify-write.
- device: Aries Device struct
- orientation: Orientation; only bit 0 is used
Returns: AriesErrorType - Aries error code

# ariesPipeRxAdapt
Run receiver adaptation on a lane: request Rx EQ evaluation, wait for PhyStatus, then retry up to 20 times for RxValid. If RxValid stays 0 it only logs an error and still returns success.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
Returns: AriesErrorType - Aries error code

# ariesPipeFomGet
Read a lane's FoM feedback byte.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- fom: FoM value
Returns: AriesErrorType - Aries error code

# ariesPipeRxStandbySet
Override a lane's PIPE RxStandby signal.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: RxStandby value
Returns: AriesErrorType - Aries error code

# ariesPipeTxElecIdleSet
Set TxElecIdle with TxDataValid asserted around the change, as the SDK's comment requires. The write goes to the opposite side's path, and the register is per path, so it covers both lanes in the pair.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: TxElecIdle value
Returns: AriesErrorType - Aries error code

# ariesPipeRxEqEval
Override a lane's PIPE RxEqEval signal.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: RxEqEval value
Returns: AriesErrorType - Aries error code

# ariesPipeRxEqInProgressSet
Override a lane's PIPE RxEqInProgress signal.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: RxEqInProgress value
Returns: AriesErrorType - Aries error code

# ariesPipePhyStatusClear
Clear a lane's captured PhyStatus flags. PhyStatus is captured in the opposite side's path.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
Returns: AriesErrorType - Aries error code

# ariesPipePhyStatusGet
Read a lane's captured PhyStatus flags from the opposite side's path.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: True if either flag is set
Returns: AriesErrorType - Aries error code

# ariesPipePhyStatusToggle
Despite the name, this only waits: it polls PhyStatus up to 100 times. On timeout it logs an error and still returns success.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
Returns: AriesErrorType - Aries error code

# ariesPipePowerdownSet
Change a path's PIPE power state, and do nothing if it is already there. Going from P0 to P1 puts the transmitters in electrical idle first; going from P1 to P0 takes them out afterwards.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: Power state: P0 (0) or P1 (2)
Returns: AriesErrorType - Aries error code

# ariesPipePowerdownCheck
Compare the PMA Tx and Rx power states with `value`. A mismatch or unsupported value is only logged, so this still returns success.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: Expected power state: P0 (0) or P1 (2)
Returns: AriesErrorType - Aries error code

# ariesPipeRateChange
Change the PIPE rate if it differs from `rate`. This puts `lane` and `lane + 1` in Rx standby and the transmitters in electrical idle, and it does not undo either; the SDK's comment says that step happens in test-mode Rx setup. Returns `ARIES_INVALID_ARGUMENT` for rates outside 1 to 5.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- rate: PCIe generation, 1 to 5
Returns: AriesErrorType - Aries error code

# ariesPipeRateCheck
Compare the PMA Tx and Rx rates with the values expected for `rate`. A mismatch is only logged, so this still returns success. The Rx mismatch message is suppressed for Gen3.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- rate: PCIe generation, 1 to 5
Returns: AriesErrorType - Aries error code

# ariesPipeDeepmhasisSet
Set a lane's Tx de-emphasis. The misspelled name is the SDK's. If `de` is not -1 it is used as the raw value. Otherwise, if `preset` is not -1, a preset from 0 to 10 is used, and any other preset gets preset 5's coefficients. Otherwise the value comes from `pre`, `main` and `pst`, in bits 5:0, 11:6 and 17:12.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- de: Raw value, or `ARIES_PIPE_DEEMPHASIS_DE_NONE` (-1)
- preset: Preset, or `ARIES_PIPE_DEEMPHASIS_PRESET_NONE` (-1)
- pre: Pre-cursor coefficient
- main: Main cursor coefficient
- pst: Post-cursor coefficient
Returns: AriesErrorType - Aries error code

# ariesPipeRxPolaritySet
Override a lane's PIPE Rx polarity.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: Polarity; only bit 0 is used
Returns: AriesErrorType - Aries error code

# ariesPipeRxTermSet
Write a lane's PCS Rx termination register.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: Termination value
Returns: AriesErrorType - Aries error code

# ariesPipeBlkAlgnCtrlSet
Write a path's PIPE BlockAlignControl override.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number; the path's even lane is used
- value: BlockAlignControl value
- enable: Override enable
Returns: AriesErrorType - Aries error code

# ariesPMABertPatChkSts
Read a lane's PRBS checker error count. The register is read twice, as the SDK requires. Counts of 32768 or more are scaled, and the count saturates at 4194176.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- ecount: Error count
Returns: AriesErrorType - Aries error code

# ariesPMABertPatChkToggleSync
Pulse the checker's SYNC bit, bit 4 of `RX_LBERT_CTL`. The SDK calls this twice in a row to clear the error counter.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
Returns: AriesErrorType - Aries error code

# ariesPMABertPatChkDetectCorrectPolarity
If the error count is saturated at 4194176, flip the lane's Rx polarity and clear the counter. Otherwise it changes nothing.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
Returns: AriesErrorType - Aries error code

# ariesPMARxInvertSet
Set a lane's Rx invert value and its override enable.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- invert: Invert value
- override: Override enable
Returns: AriesErrorType - Aries error code

# ariesPMABertPatChkConfig
Set a lane's PRBS checker pattern, retrying up to 100 times. If the setting does not take, it only logs warnings and still returns success.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- mode: Pattern, or `DISABLED`
Returns: AriesErrorType - Aries error code

# ariesPMABertPatGenConfig
Set a lane's PRBS generator pattern, disabling the generator first. It then always makes 20 check passes. If the setting does not take, it only logs warnings and still returns success.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- mode: Pattern, or `DISABLED`
Returns: AriesErrorType - Aries error code

# ariesPMARxDataEnSet
Force a lane's Rx data enable on when `value` is true. When false, it clears both the value and override bits.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: Enable
Returns: AriesErrorType - Aries error code

# ariesPMATxDataEnSet
Force a lane's Tx data enable on when `value` is true. When false, it clears both the value and override bits.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- value: Enable
Returns: AriesErrorType - Aries error code

# ariesPMAPCSRxRecalBankOvrdSet
Set a lane's Rx recalibration bank override. The override is always enabled, with its value set by `enable`. The SDK's comments mark true as Gen3/4 and false as Gen5.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- enable: True for Gen3/4, false for Gen5
Returns: AriesErrorType - Aries error code

# ariesPMAPCSRxReqBlock
Set or clear bit 5 of a lane's `PCS_XF_ATE_OVRD_IN` register. `ariesPipeRxAdapt` clears it before adaptation and sets it again while waiting for RxValid.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- enable: Bit value
Returns: AriesErrorType - Aries error code

# ariesPMAVregVrefSet
Set a lane's Rx regulator settings for a data rate. When `device->revNumber` is 1 (A0), the split is Gen1-2 and Gen3-5, and the shared Vref is also set from lanes on a PMA boundary. Otherwise (B0) the split is Gen1-3 and Gen4-5.
- device: Aries Device struct
- side: PMA side: B (0), A (1)
- lane: Absolute lane number
- rate: PCIe generation
Returns: AriesErrorType - Aries error code

# asteraI2COpenConnection
Open `/dev/i2c/N`, or `/dev/i2c-N` if that does not exist, and set the slave address. This is a weak default in `platform.c`, so a platform can replace it. It returns the handle, which is negative on failure.
- i2cBus: Bus number
- slaveAddress: 7-bit slave address
Returns: int - file handle

# asteraI2CWriteBlockData
Send an SMBus block write. This is a weak default in `platform.c`, so a platform can replace it.
- handle: I2C handle
- cmdCode: SMBus command code
- numBytes: Number of bytes to write
- buf: Data to write
Returns: Result of `i2c_smbus_write_block_data`, negative on error

# asteraI2CReadBlockData
Send an SMBus block read. This is a weak default in `platform.c`, so a platform can replace it. `numBytes` is ignored unless the SDK is built with `SMBUS_BLOCK_READ_UNSUPPORTED`, which uses an I2C block read instead.
- handle: I2C handle
- cmdCode: SMBus command code
- numBytes: Number of bytes to read
- buf: Buffer for the data
Returns: Number of bytes read, negative on error

# asteraI2CWriteReadBlockData
Send an SMBus block process call, a write and read in one transaction. This is a weak default in `platform.c`, so a platform can replace it.
- handle: I2C handle
- cmdCode: SMBus command code
- numBytes: Number of bytes to write
- buf: Data to write; also receives the response
Returns: Result of `i2c_smbus_block_process_call`, negative on error

# asteraI2CBlock
Take an exclusive `flock` on the handle. This is a weak default in `platform.c`, so a platform can replace it.
- handle: I2C handle
Returns: 0 on success, otherwise a negative errno

# asteraI2CUnblock
Release the `flock` on the handle. This is a weak default in `platform.c`, so a platform can replace it.
- handle: I2C handle
Returns: 0 on success, otherwise a negative errno
