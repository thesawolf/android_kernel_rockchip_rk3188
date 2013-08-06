RK3188 kernel builder script
----------------

This script is designed to simplify some aspects of the kernel building
process for RK3188 kernels. It is primarily linked for usage with my
own RK3188 kernel tree, but will ultimately be usable in any RK3188 
kernel tree (with some modifications, as noted in script comments and
below).

LABS (or Lazy-Ass Build Script) is Copyleft (http://www.gnu.org/copyleft/)
and you are welcome to use it any way you see fit as long as you don't 
rebrand it as your own. If you do, at least mention me somewhere (since I 
laid the foundation for you). If not, may the fleas of a thousand camels 
infest your crotch and may your arms be too short to scratch :)

This script is a work in progress as RK3188 devices will HOPEFULLY get 
added to it all the time.

**Builder features:**
- Toolchain selection
- Config builder
- Output specification
- Build log options
- Clean up options
- Thread selection
- Save/Reload build settings (for easy reuse)
- Commandline run (no need to use menu, just specify -b and will pull your 
saved build settings)
- Auto builder (based on your options selected, will handle all the make 
calls for you)

**Device features:**
- predefined device selection (or gets you started with a template)
- HDMI resolution selector
- LCD settings
- Wifi chipset selector
- Bluetooth chipset selector
- PMU chipset selector
- Specify any predefined special voltages
- Overclocking options

**Current Devices supported:**
- Tronsmart MK908
- Tronsmart T428
- Imito QX-1

--------------------

**INSTRUCTIONS:**
- run ./BUILDIT.sh from commandline in a RK3188 kernel tree
- commandline opts of: -v for vers, -b for unattended build, -?/h for help,
no parameters specified will run LABS through the menu system

**NOTES:**
A number of features are linked to my kernel tree (ie. the toolchains, etc.)
It doesn't take much to edit the script to reflect your personal tree if you 
don't use mine. Script is commented enough to help you find what you need.
Notably, some of the options are linked to source files in arch/arm/mach-rk3188
that I have modified to handle as kernel config options (so you don't have to
constantly flag the define tags in the source file). However, if you are using
your own kernel source, you will need to look at my Kconfig and 
board-rk3188-box.c (look for the SAW-tagged comments) and mimic some of them to
achieve the same results (for now, anyways)

-------------------

**REVISION HISTORY:**
- (2.0) based off build_mk908.sh (LABS 1.5) in my kernel source
- created new repo just for it, now LABS 2.1 and called buildit.sh
- (2.2) features added, more checkers, added QX1
- (2.3) config builder created and linked and menus updated
- config builder moved to separate script, not meant to be run alone
- renamed scripts.. just because (now BUILDIT.sh and CFGCORE.sh)
- (2.4) added in debugging, Mali and exFAT kernel options (if available)
- major fleshing out in the config builder (CFGCORE.sh) and running it by itself
 will cause the script to generate an example defconfig (EXAMPLE-CONFIG)
- (2.5) config builder pretty much finished and made alot of attempts to make
it kernel source neutral where I could
- Build routines (unattended and in script working properly now)

**TODO:**
- config builder mailer. If you customize your device config for a device 
not on the list, have it email to me for inclusion in the script or you can
just take the generated defconfig and get it to me
- <del>finish tweaking config builder to reflect builder options</del>
- <del>get unattended building working under new system</del>

**CREDITS:**
- Me (duh)
- phjanderson (@freaktab) for making the suggestion and T428 config
- Leolas (@freaktab) QX-1 config 
- Your name could go here if/when you submit a new device :)
