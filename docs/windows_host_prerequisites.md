# Windows host prerequisites and API approval

This document defines the supported Windows build and device-driver boundary
for the native `picosd-host` application. It is an approval of APIs already
provided by Windows, not an approval to vendor an SDK, driver, runtime, or USB
library into this repository.

## Supported environment

- Use a supported 64-bit Windows 10 or Windows 11 installation.
- Install CMake 3.20 or newer and a C++17-capable MSVC toolchain. The **Desktop
  development with C++** workload from Visual Studio 2022 supplies the compiler,
  standard library, Windows SDK headers, and import libraries used by the host.
- Configure with CMake's Visual Studio generator from a Developer Command Prompt,
  or allow CMake to locate an installed Visual Studio instance. PicoSDEmu does
  not require the Pico SDK to build the host application.

Only the C++ standard library and Windows SDK are approved for the Windows host.
Adding a package manager, redistributable runtime, USB library, installer
framework, or bundled toolchain requires a separate dependency review.

## USB CDC driver boundary

The Pico firmware presents a USB CDC abstract-control-model serial interface.
Windows must bind that interface to Microsoft's inbox `usbser.sys` driver and
expose it as a COM port. No project-owned kernel driver or third-party INF is
required or approved. Confirm the binding and assigned port in Device Manager;
the command line accepts the resulting name, for example `COM7`.

If Windows shows an unknown device, fix the firmware descriptors or driver
binding rather than downloading an arbitrary serial driver. Driver installation
and Device Manager changes may require administrator access, but running
`picosd-host` normally should not.

## Approved native APIs

The following Windows SDK API surface is approved for the current host:

| Area | Approved API | Project use |
| --- | --- | --- |
| CDC transport | `CreateFile`, `CloseHandle`, `GetCommState`, `SetCommState`, `SetCommTimeouts`, `ReadFile`, `WriteFile` | Open an assigned `COM` device exclusively, configure byte-oriented communication, and exchange bounded protocol lines. |
| Image ownership | `CreateFileW`, `LockFileEx`, `UnlockFileEx`, `CloseHandle` | Hold an immediate exclusive lock for the lifetime of a mounted image. |
| Image data and flush | C++17 file streams backed by the MSVC standard library | Perform binary positional reads/writes and explicitly flush stream buffers. |

COM names are converted internally to the `\\.\COMx` device-path form so ports
above `COM9` work. The transport requests a zero-share handle; another process
that owns the port therefore causes a clean open failure. The configured
`115200 8N1` state satisfies the Windows communications API, although USB CDC
transfers are carried by USB rather than a physical UART baud clock.

The approved scope deliberately excludes SetupAPI device enumeration, WinUSB,
libusb, overlapped I/O, background service installation, and custom driver
packages. Any future use of those facilities needs a design update, tests, and
an entry in the dependency/reference register first.

## Operator checks

1. Build `picosd-host` and run its native tests before attaching hardware.
2. Connect the Pico and verify that Device Manager reports a Microsoft USB
   serial device with a stable `COM` name.
3. Run `picosd-host --port COM7 status`, substituting the assigned port.
4. If opening fails, close terminal programs that may own the port and retry.
5. Keep the served image on a filesystem that honors Windows byte-range locks;
   do not serve the same image from a second process.

These checks establish the software prerequisites only. Hardware release still
requires the disconnect, stale-handle, access-denial, long-path, interruption,
and clean-shutdown cases in Phase 5 of the implementation plan.

## Upstream documentation

- Microsoft: [USB serial driver (`Usbser.sys`)](https://learn.microsoft.com/windows-hardware/drivers/usbcon/usb-driver-installation-based-on-compatible-ids)
- Microsoft: [Communications resources](https://learn.microsoft.com/windows/win32/devio/communications-resources)
- Microsoft: [`CreateFile` communications configuration](https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-createfilea#communications-resources)
- Microsoft: [`LockFileEx`](https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-lockfileex)
- CMake: [Visual Studio 17 2022 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html)
