Linux (Android-specific) kernel release 3.0.36+ for rockchip RK3188 (MK908)
------------

This is a Android-specific Linux kernel for the Rockchip RK3188-based MK908.
Tried to compile in enough generic RK3188 stuff to potentially handle other
RK3188 devices, but currently it's optimized for MK908s. Device support may
have been added into kernel, but that doesn't necessarily means it's working
and will probably require further testing to confirm. Feature confirmation
is always welcome. Feature requests are welcome, within reason (if it doesn't
normally work on linux or android, I'm not going to be able to make it
happen as writing device drivers from scratch is beyond my current scope)

To be installed into: (android)/kernel/rockchip/rk3188/

**Kernel Features (highlight rundown of kernel config):**
- Default hostname set to MK908 (network identity choice, change if you want)
- LZO Kernel compression (not the best compression, but one of the fastest)
- Kernel .config support (via /proc/config.gz) so you can extract config
- Module versioning so modules can be ported to other kernels
- Support for 2TB+ devices/files
- I/O Schedulers (Noop [default], Deadline, CFQ, SIO, VR, Zen)
- Lastlog support (for debugging, via /proc/last_log)
- Serial debugging support
- 4-cores supported in kernel
- CPU Governors (Interactive [default], Performance, Powersave, Userspace,
OnDemand, Conservative, SmartAssV2, MinMax)
- NEON extension support
- TCP/IPv4-v6 networking with multicasting, routing, tunneling, IPsec, 
filtering and L2TP support
- Full complement of Bluetooth functions/drivers
- RF switch support (for WiFi and Bluetooth)
- NAND support with Rockchip-specific, generic and page write checking
- Ethernet (10/100/1000/10000) support
- WiFi devices (on-board & USB)support & AP6210-specific wifi module (MK908)
- Input device support (keyboard, mouse, joystick/gamepad)
- Xbox/Xbox360/PS3 controller support
- (hardware monitoring left out for now because rockchip source has some 
hardcoded values in place for some reason [ie. CPU temperature])
- Voltage/Current regulator support (PMIC)
- Remote control support (must be dongle or mod)
- various USB Tuner/Camera support
- updated Mali400 support (profiling, UMP, debug, shared interrupts)
- some frame buffer support 
- Rockchip frame buffer support (mirroring, lcdc0 and lcdc1)
- 480p/720p [default]/1080p support - 720/1080 kernels will be made
- Rockchip-specific HDMI support (debugging with Silicon Image 
SiI9022A/9024A and ITE IT66121 [MK908-specific] HDMI transmitter support
- ALSA (sound) support (ARM, USB and Rockchip specific devices)
- full complement of USB HID, printer, Mass storage, Serial and mobile modem
devices support via regular usb port and OTG
- Ext2, Ext3, Ext4, ISO9660 CDROM, UDF, VFAT, NTFS (w/write), YAFFS2,
proc, NFS, CIFS filesystem support
- UPDATE: YAFFS2 disabled for code debugging (for now)
- UPDATE: exFAT support removed from kernel, moved to fuse-exfat
- CPU/GPU overclocking support (not a kernel config option, built-in)

**Revision History:**
- Initial commit, honestly have no idea of the source as it was sent to me
and doesn't reflect some of the stuff I've seen in aloksinha's or omegamoon's
RK3188 kernel sources. It could simply be a hybrid. If someone can spot it's 
origin, please let me know so I can credit them properly. Some old git info
carried over saying htc authored but I have no idea if that's where the base
was from. (determined that's it the original Rikomagic source)
- Added some omegamoon defconfigs & build stuff because I'm lazy.
- Added latest Mali GPU drivers (r3p2-01rel2 from 31 May 2013)
- Added VR and SIO I/O schedulers
- Added SmartAssV2 and MinMax CPU governors
- Added prelim CPU o/c up to 1920 MHz with initial GPU o/c (needs tweaking)
- Added Zen I/O scheduler
- Added exFAT filesystem support
- Disabled exFAT and YAFFS2 to debug some code, still in source, just not
enabled in config. Enabling will cause some errors, atm.
- Extended build script to move kernel/modules into device directories
- Updated some rockchip/fb source from leolas, galland sources
- Removed exFAT kernel support due to too many issues and old backport,
moved to fuse-exfat module in device build
- Restructure buildscript (build_mk908) to handle menu system so I can
do specific kind of builds since I keep losing track of stuff :P
- Determined origin of my kernel source finally. it's the original source
from Rikomagic before alok's linux-centric source changes
- MAJOUR build script overhaul.. it's grown into quite a lazy-ass beast
- FINALLY got a working config (TESTCONFIG) with Wifi AND BT working now
- ALOT of O/C testing, tweaking (thanks to Sam321 for alot of the legwork)
- Vsync FIX by phjanderson! (suck it Strauzo)
- New build script version (now supports saving config options for quicker rebuilds!)

**ToDo:**
- Linaro toolchain build (added 4.6 and 4.8 added, not tested yet tho)
- YAFFS2 debugging
- more O/C stuff.. get running more stable and finally unlock GPU O/C
- isolate issue with cpufreq not reporting to sys properly for apps
- DDR init issue with improper timings setting and freq init
