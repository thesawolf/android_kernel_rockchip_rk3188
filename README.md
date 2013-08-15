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
- I/O Schedulers (Noop [default], Deadline, CFQ, SIO, VR, Zen, Row, BFQ)
- Lastlog support (for debugging, via /proc/last_log)
- Serial debugging support
- 4-cores supported in kernel
- CPU Governors (Interactive [default], Performance, Powersave, Userspace,
OnDemand, Conservative, SmartAssV2, MinMax, Adaptive, BrazilianWax, Hyper, Intellidemand, InteractiveX, OnDemandX, SavagedZen, Lionheart, LulzActive)
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
- CPU/GPU overclocking support (thanks to Sam321 @freaktab)
- UPDATE: a better exFAT driver added (see fs/exfat for more details tho!)
- Frame skipping fix (thanks to phjanderson @freaktab)

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
- ALOT of O/C testing, tweaking (thanks to Sam321 @freaktab)
- Vsync FIX by phjanderson @freaktab (suck it Strauzo)
- New build script version (now supports saving config options for quicker rebuilds!)
- Added ROW I/O Scheduler, preliminary BFQ I/O (still needs tuning)
- Added Adaptive, BrazilianWax, Hyper, Intellidemand, InteractiveX, Lionheart, LulzActive, MinMax (now working), OnDemandX, and SavagedZen CPU Governors
- Build script updated (removed auto screen sync defconfigs)
- Updated defconfigs with latest working TESTCONFIG
- Figured out the DDR init issue (somewhat). Can't seem to init frequency in userspace but can init in kernel config. Setting via kernel for now. Will come back to it later. Can change suspend and video freqs in board-rk3188-box.c but normal and reboot not affected.
- Added exFAT support again from better source (see fs/exfat for details tho!), tested with small files read/write to exFAT-formatted USB flash drive, works.
- Toolchains added for convenience and build script
- Updated Linaro 4.8 toolchain to 2013.07 release, tested. 
- Device building script forked from build_mk908 (now called buildit)
- Added in 2.4 version of LABS (BUILDIT.sh/CFGCORE.sh) to replace build_mk908
- Stashed away old files in temp directory as LABS is almost ready to roll
- Added in final 2.5 version of LABS
- Power on light CPIO found in MK908 SDK1 leak, patched
- Added in undervolting support
- Updated LABS to 2.7 version (w/undervolt check and device neutral checks)
- updated Mali drivers to r3p2-01rel3 (13 Aug 2013 release)

**ToDo:**
- YAFFS2 debugging
- <del>more O/C stuff.. get running more stable and finally unlock GPU O/C</del>
- isolate issue with cpufreq not reporting to sys properly for apps
- <del>DDR init issue with improper timings setting and freq init (defaults to 300MHz instead of the stock 396MHz)</del>
- <del>I want my blue power on light back (mk908)</del>
- <del>test the new exFAT driver</del>
- Undervolt define testing and tweaking to achieve real undervolting
- Mali driver testing on custom ROM

**Thanks:**
- Finless for preparing the way
- neomode for freaktab and making all the info available
- leolas for all your help along the way
- Sam321 for your o/c work
- phjanderson for your frameskipping/vsync fix
- linuxium for the kernel sources
- galland and omegamoon for various kernel fixes
- crewrktablets for various models and tools to work from/with
- hoabycsr for various tips/tricks along the way
- rxrz for the exFAT support (via samsung leak)
- androidminipc62 (@armtvtech) for the MK908 SDK1 leak
- and misc others that contributed to making all this happen
(if I forgot anyone specific, sorry.. remind me, I don't mind giving credit where credit is due)
