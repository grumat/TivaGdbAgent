Windows version of the TI Tiva/Stellaris lm4tools
=================================================

## Target

First, you have to know that this software was tested only on the TI EK-TM4C123GXL LaunchPad board. Those boards use a chip to translate from USB to JTAG commands. The chip is called ICDI and it implements a GDB compatible link using a standard WinUSB driver.

**Note:** At that moment, the tool was tested on Windows 10 only.

## Installation

This code is only intended for Windows and it is on a _alpha state_ and no release is currently available.

The tool uses the standard TI drivers (WinUSB.sys). Just use the [drivers supplied by TI (spmc016a)](http://www.ti.com/tool/stellaris_icdi_drivers).

The driver installs:
 - USB Composite Device (VID_1CBE&PID_00FD)
 - Stellaris Virtual Serial Port (VID_1CBE&PID_00FD&MI_00)
 - Stellaris ICDI DFU Device (VID_1CBE&PID_00FD&MI_03)
 - Stellaris ICDI JTAG/SWD Device (VID_1CBE&PID_00FD&MI_02)

## Installation from source (advanced users)

Use a Visual Studio 2017 Community Edition (or better) to compile the project.

## Using the gdb server

To run the gdb server:

```
> TivaGdbAgent.exe
```

The GDB server waits for a connection at the port **localhost:7777**.

Then, in your project directory, something like this:

```
> arm-none-eabi-gdb blink.elf
...
(gdb) tar extended-remote :7777
...
(gdb) load
Loading section .text, size 0x458 lma 0x8000000
Loading section .data, size 0x8 lma 0x8000458
Start address 0x00000000, load size 109288
Transfer rate: 17 KB/sec, 6071 bytes/write.
(gdb)
...
(gdb) continue
```

## Resetting the chip from GDB

You may reset the chip using GDB if you want. You'll need to use `target
extended-remote' command like in this session:

```
(gdb) target extended-remote localhost:7777
Remote debugging using localhost:7777
0x080007a8 in _startup ()
(gdb) kill
Kill the program being debugged? (y or n) y
(gdb) run
Starting program: /home/bally/firmware.elf
```

Remember that you can shorten the commands. `tar ext :7777' is good enough
for GDB.

## Running programs from SRAM

You can run your firmware directly from SRAM if you want to. Just link
it at 0x20000000 and do

```
(gdb) load firmware.elf
```

It will be loaded, and pc will be adjusted to point to start of the
code, if it is linked correctly (i.e. ELF has correct entry point).

## Writing to flash

The GDB stub ships with a correct memory map, including the flash area.
If you would link your executable to `0x08000000` and then do

```
(gdb) load firmware.elf
```

then it would be written to the memory.

## FAQ

TBD.

## Known missing features

Some features are missing from the `grumat/TivaGdbAgent` project and we would like you to help us out if you want to get involved:

 * Test on Windows 7
 * Flash only tool (use [LM Flash Programmer](http://www.ti.com/tool/lmflashprogrammer) for now)

## Known bugs

TBD.

## Contributing and versioning

* When creating a pull request, please open first a issue for discussion of new features. Bugfixes don't need a discussion.

## License

The stlink library and tools are licensed under the [BSD license](LICENSE).

The WinUSBWrappers source files uses a [custom freeware license](http://www.naughter.com/winusbwrappers.html).
