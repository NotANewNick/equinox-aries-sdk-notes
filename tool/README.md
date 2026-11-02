# equinox_diag

Command-line diagnostics for the Aries PT5161L retimer on the EQUINOX-REVC card, built on the unmodified Aries C SDK. Unofficial.

**Not tested on real hardware.** It has only run against the simulated chip in `test/`. The simulator models the bus protocol, not PCIe, so it can't show that measurements are correct.

## Build

You need `gcc`, `make` and the Linux kernel headers. `libi2c` and OpenBMC are not needed.

```
make
make test
```

The binary is `build/equinox_diag`. If the tool says your adapter lacks SMBus block reads, rebuild with `make clean && make NO_BLOCK_READ=1`.

## Use

The retimer's SMBus header is 1.8 V, and its pinout is not public. Connect a 1.8 V adapter, then find the address and bus:

```
sudo modprobe i2c-dev
i2cdetect -l
sudo i2cdetect -y -r N 0x20 0x27
```

Then run a command, replacing `N` and the address:

```
sudo ./build/equinox_diag --bus N --addr 0x24 info
sudo ./build/equinox_diag --bus N --addr 0x24 links
sudo ./build/equinox_diag --bus N --addr 0x24 --port up --confirm eye
```

| Command | Output | Effect on the card |
| --- | --- | --- |
| `info` | Firmware, IDs, health, temperature | Changes no link or firmware settings |
| `links` | State, speed, width, recoveries, minimum FoM | Changes no link or firmware settings |
| `eye` | Per-lane eye margins, written to `margin_<port>.csv` | Margining may disturb a live link, so it needs `--confirm` |

**If `eye` prints `ACK timed out`, discard the CSV.** The SDK logs that error but still carries on, so the file can look valid. Each timeout can also take a minute or more.

Port 0 in the CSV name is upstream and port 1 is downstream. `eye` refuses to run unless every link is up. It always measures all 16 lanes. Use a dedicated adapter rather than a PC's shared SMBus, and don't run this while the `pt5161l` kernel driver is bound to the chip.

## What it writes to the chip

- **Every command:** the SDK's start-up sequence writes to the chip. Besides address selection and read handshakes, it runs an eFuse *read* procedure using registers 0x8EC and 0x8F6 to fetch temperature calibration. In simulation, that leaves the eFuse clock-select bit (0x8EC bit 25) set. The SDK contains no fuse-programming code.
- **ARP:** blocked. `ariesInitDevice()` normally tries to reassign the chip's address through 0x61 if a read fails. This tool refuses to open 0x61.
- **Audit:** `make test` decodes every bus write made by `info` and `links`, and fails if anything else is written.
- **Not audited:** `eye`, which writes to the chip by design.

The exit code is 0 on success, 1 for a device or link problem, and 2 for a usage error.

## Files

| Path | Purpose |
| --- | --- |
| `equinox_diag.c` | The tool |
| `compat/openbmc/obmc-i2c.h` | Replaces OpenBMC's I2C library so the SDK builds on ordinary Linux |
| `test/fake_aries.c` | Simulated chip |
| `test/run_tests.sh` | Test suite |
