#!/bin/bash

VERS="1.5"
# LAZY-ASS BUILD SCRIPT (LABS) by
# Thesawolf (thesawolf [at] gmail [d0t] com)
#
# Copyleft(c)2013, it's JUST a script, but if you take
# this and rebrand it as YOUR own or don't mention *I*
# created it, then YOU are a scumbag and karma's a bitch
# --------------------------------------------------------
# NOTE: This build script is tailoured to *MY* build
# environment, some options may not work for you unless
# you are using a similar setup or one pulled specifically
# from my githubs (kernel, device, toolchains, etc.)
#
# UPDATE (04AUG2013): Attempting to make this build script
# more device-friendly per a community suggestion by 
# phjanderson (@freaktab). Let's see where this goes..
#
# and YES.. I KNOW that some might consider this a bit
# overkill but it just GREW into this script and I enjoy
# being lazy (and I know that there are ways to optimize
# this script or better ways to do it.. but it works for
# me and I'd like to refer you to the script name.. LOL)
# --------------------------------------------------------
#
# Revision history:
# - framework (output, logs, build, threads, toolchains)
# - menu system in place
# - added extra toolchains selection
# - added menuconfig options
# - added custom menuconfig loader
# - output custom type/location
# - clean-up options
# - RKCRC option
# - added zkernel.img type to custom output
# - .config to chosen defconfig after using menuconfig
# - added default selections as well to custom dirs
# - oops.. RKCRC NOT needed for standalone kernels for RK
# - added in config options file for easy rebuilding
# - reload/save config options from file
# - commandline arguments (help, version, build)
# - device-neutral build system (hopefully)
# - create configs on the fly for building, no more storage

# initialize default settings 
function initdef 
{
# determine threads using count cpu/cores x 2
if [ -z "$THREADS" ]; then
 THREADS=$(nproc)
 THREADS=$(($THREADS*2))
fi

# set defaults if not set in env
if [ -z $TOOLCHAIN ]; then 
 TOOLCHAIN=1
 TCDESC="System (arm-linux-gnueabi)"
fi

if [ -z $CROSS_COMPILE ]; then
 CROSS_COMPILE=arm-linux-gnueabi-
fi

if [ -z $DEVICE ]; then
 DEVICE=mk908-720-debug-defconfig
fi

if [ -z $OUTLOC ]; then
  KERNPARM1="kernel.img"
  KERNPARM2="./NEW"
  OLDESC="kernel.img > KERNEL/MODULES dirs"
  OUTLOC=5
#  RKCRC="no"
fi

if [ -z $LOGIT ]; then
  LOGIT=2
  LOGPARMA="echo '------------------------NEW-KERNEL---------------------' >BUILD.LOG"
  LOGPARM1='2>&1 | tee -a BUILD.LOG'
  LOGPARMB="echo '------------------------NEW-MODULES--------------------' >>BUILD.LOG"
  LIDESC="BUILD.LOG/screen output (replace)"
fi

if [ -z $CUTIME ]; then
   CUTIME=3
   CUDESC="FULL clean-up (make mrproper)"
   CUPARM="make ARCH=arm mrproper"
fi
}

# import config file options, if found
# turned into function so can be called from menu or cmdline
function icfig
{
 if [ -e build_mk908.cfg ]; then
    echo "Config file found, importing..."
    . build_mk908.cfg
 else
    echo "No config file found, skipping..."
 fi
}

# exit function, nothing special
function outtahere
{
 clear
 echo
 echo "Thanks for using my Lazy-Ass Build Script :) Hope it helped! - Thesawolf"
 echo 
 break
}

# devices menu, this will be updated with new devices *after* device 
# requirements have been verified *and* tested
function devitems
{
 devopt=""
 while [[ "$devopt" != "X" || "$devopt" != "x" ]]
 do
	clear
	echo
	echo "   =============[ DEVICES MENU ]============="
	echo "   1. MK908"
	echo "   =----------------------------------------="
	echo "     (don't see your device? get it added!)"
	echo "   =----------------------------------------="
	echo "   0. Continue to LABS"
        echo 
	echo "   >> X. I'm confused, get me out of here!"
	echo 
	read -n 1 -p "   Choose device building for: " devopt
	if [ "$devopt" = "1" ]; then
		break
	elif [ "$devopt" = "0" ]; then
		break
	elif [[ "$devopt" = "X" || "$devopt" = "x" ]]; then
		outtahere
	fi
 done
}

# toolchains menu, you can change this to different ones you might have
# installed on YOUR system by modifying the menu and then its
# corresponding option afterward CROSS_COMPILE is the location on your
# system in relation to the kernel directory (system-wide installed ones
# just get named directly)
function tcitems 
{
 tcopt=""
 while [ "$tcopt" != "0" ]
 do
	clear
	echo
	echo "   ===========[ TOOLCHAINS MENU ]==========="
	echo "    Current: $TOOLCHAIN"
	echo "   =---------------------------------------="
	echo "   1. System (arm-linux-gnueabi via apt-get)"
	echo "   2. Default Android (arm-eabi-4.4.3)"
	echo "   3. Linaro 4.6.2 (arm-eabi-linaro-4.6.2)"
	echo "   4. Linaro 4.8 (android-toolchain-eabi)"
	echo "   5. Google (arm-linux-androideabi-4.7)"
	echo "   ========================================="
	echo "   >> 0. EXIT"
	echo
	read -n 1 -p "   Select Option: " tcopt
	if [ "$tcopt" = "1" ]; then
	    TOOLCHAIN=1
	    CROSS_COMPILE=arm-linux-gnueabi-
	    TCDESC="System (arm-linux-gnueabi)"
	   break
	elif [ "$tcopt" = "2" ]; then
	    TOOLCHAIN=2
	    CROSS_COMPILE=../../../device/rockchip/rk3188/toolchains/arm-eabi-4.4.3/bin/arm-eabi-
	    TCDESC="Android (arm-eabi-4.4.3)"
	   break
	elif [ "$tcopt" = "3" ]; then
	    TOOLCHAIN=3
	    CROSS_COMPILE=../../../device/rockchip/rk3188/toolchains/arm-eabi-linaro-4.6.2/bin/arm-eabi-
	    TCDESC="Linaro 4.6.2 (arm-eabi-linaro-4.6.2)"
	   break
	elif [ "$tcopt" = "4" ]; then
	    TOOLCHAIN=4
	    CROSS_COMPILE=../../../device/rockchip/rk3188/toolchains/android-toolchain-eabi/bin/arm-eabi-
	    TCDESC="Linaro 4.8 (android-toolchain-eabi)"
	   break
	elif [ "$tcopt" = "5" ]; then
	    TOOLCHAIN=5
	    CROSS_COMPILE=../../../device/rockchip/rk3188/toolchains/arm-linux-androideabi-4.7/bin/arm-linux-androideabi-
	    TCDESC="Google (arm-linux-androideabi-4.7)"
	   break
	elif [ "$tcopt" = "0" ]; then
	   break
	fi
 done
}

# just a reminder to "pop-up" before entering the menuconfig
function poprem
{
 clear
 echo "Loading ($DEVREM) into .config..."
 echo 
 echo "REMINDER: If you make any changes and choose to save the .config,"
 echo " the .config will be copied over : $DEVREM"
 echo " If you save the configuration to another file be sure to load"
 echo " that new config file in the configs menu when you are done!"
 echo
 read -n1 -p ">> Press any key to continue.."
}

# device configs menu, these are all my preset ones with an option to 
# edit them and now you can load your own
function cfgitems
{
 cfgopt=""
 while [ "$cfgopt" != "0" ]
 do
	clear 
	echo
	echo "   ==================[ CONFIGS MENU ]=================="
	echo "    Current: $DEVICE"
	echo "   =--------------------------------------------------="
	echo "   1. mk908-720-debug-defconfig        <- Q. menuconfig"
	echo "   2. mk908-1080-debug-defconfig       <- W. menuconfig"
	echo "   3. mk908-720-defconfig              <- E. menuconfig"
	echo "   4. mk908-1080-defconfig             <- R. menuconfig"
        echo "   =--( Overclock kernels )---------------------------="
	echo "   5. mk908-720-OC-debug-defconfig  <- T. menuconfig"
	echo "   6. mk908-1080-OC-debug-defconfig <- Y. menuconfig"
	echo "   7. mk908-720-OC-defconfig        <- U. menuconfig"
	echo "   8. mk908-1080-OC-defconfig       <- I. menuconfig"
	echo "   =--------------------------------------------------="
	echo "     9. make menuconfig (with no defconfig set)"
        echo "   =--------------------------------------------------="
        echo "     L. load your own specified defconfig"
	echo "   ===================================================="
	echo "   >> 0. EXIT"
	echo
	read -n 1 -p "   Select Option: " cfgopt
	if [ "$cfgopt" = "1" ]; then
	   DEVICE=mk908-720-debug-defconfig
	   break
	elif [[ "$cfgopt" = "Q" || "$cfgopt" = "q" ]]; then
	   DEVREM="mk908-720-debug-defconfig"
	   poprem
	   cp $DEVREM .config
	   make ARCH=arm menuconfig
	   cp .config $DEVREM
	   break
	elif [ "$cfgopt" = "2" ]; then
	   DEVICE=mk908-1080-debug-defconfig
	   break
	elif [[ "$cfgopt" = "W" || "$cfgopt" = "w" ]]; then
	   DEVREM="mk908-1080-debug-defconfig"
	   poprem
	   cp $DEVREM .config
	   make ARCH=arm menuconfig
           cp .config $DEVREM
	   break
	elif [ "$cfgopt" = "3" ]; then
	   DEVICE=mk908-720-defconfig
	   break
	elif [[ "$cfgopt" = "E" || "$cfgopt" = "e" ]]; then
	   DEVREM="mk908-720-defconfig"
	   poprem
	   cp $DEVREM .config
	   make ARCH=arm menuconfig
	   cp .config $DEVREM
	   break
	elif [ "$cfgopt" = "4" ]; then
	   DEVICE=mk908-1080-defconfig
	   break
	elif [[ "$cfgopt" = "R" || "$cfgopt" = "r" ]]; then
	   DEVREM="mk908-1080-defconfig"
	   poprem
	   cp $DEVREM .config
	   make ARCH=arm menuconfig
	   cp .config $DEVREM 
	   break
	elif [ "$cfgopt" = "5" ]; then
	   DEVICE=mk908-720-OC-debug-defconfig
	   break
	elif [[ "$cfgopt" = "T" || "$cfgopt" = "t" ]]; then
	   DEVREM="mk908-720-OC-debug-defconfig"
	   poprem
	   cp $DEVREM .config
	   make ARCH=arm menuconfig
	   cp .config $DEVREM 
	   break
	elif [ "$cfgopt" = "6" ]; then
	   DEVICE=mk908-1080-OC-debug-defconfig
	   break
	elif [[ "$cfgopt" = "Y" || "$cfgopt" = "y" ]]; then
	   DEVREM="mk908-1080-OC-debug-defconfig"
	   poprem
	   cp $DEVREM .config
	   make ARCH=arm menuconfig
	   cp .config $DEVREM 
	   break
	elif [ "$cfgopt" = "7" ]; then
	   DEVICE=mk908-720-OC-defconfig
	   break
	elif [[ "$cfgopt" = "U" || "$cfgopt" = "u" ]]; then
	   DEVREM="mk908-720-OC-defconfig"
	   poprem
	   cp $DEVREM .config
	   make ARCH=arm menuconfig
	   cp .config $DEVREM 
	   break
	elif [ "$cfgopt" = "8" ]; then
	   DEVICE=mk908-1080-OC-defconfig
	   break
	elif [[ "$cfgopt" = "I" || "$cfgopt" = "i" ]]; then
	   DEVREM="mk908-1080-OC-defconfig"
	   poprem
	   cp $DEVREM .config
	   make ARCH=arm menuconfig
	   cp .config $DEVREM 
	   break
        elif [[ "$cfgopt" = "L" || "$cfgopt" = "l" ]]; then
           clear
           echo "Specify your own defconfig here (full filename) then press [ENTER]"
           read -e -p " (Leave blank and press [ENTER] to skip!) : " NEWDEF
           if [ "$NEWDEF" != "" ]; then
              DEVICE=$NEWDEF
           fi           
	elif [ "$cfgopt" = "9" ]; then
	   make ARCH=arm menuconfig
	elif [ "$cfgopt" = "0" ]; then
	   break
	fi
 done
}

# rkcrc "popup" question, for rockchip device flashing
# SAW - this function not necessary right now because kernels don't need CRC
function rkit
{
 clear
 echo 
 echo " Do you want to RKCRC your compiled kernel? (this is one of the"
 echo " prep steps necessary for flashing your kernel to a rockchip"
 echo " device) NOTE: If you select YES, your kernel will have an 'rk-'"
 echo " prepended to the kernel filename to indicate as such"
 echo 
 read -n 1 -p " >> Leave blank and press [ENTER] to skip!, otherwise [y|n] : " RKOPT
 if [ "$RKOPT" != "" ]; then
  if [[ "$RKOPT" = "n" || "$RKOPT" = "N" ]]; then
   RKCRC="no"
  elif [[ "$RKOPT" = "y" || "$RKOPT" = "Y" ]]; then
   RKCRC="yes"
  fi
 fi           	   
}

# output options menu, choices for where to put stuff after compiled
# default location is ./arch/arm/boot, etc.
# kernel/modules dir is ./NEW directory in kernel dir
# device directories is my rk3188 device directory (through github,
# installed in (android)/device/rockchip/rk3188/ normally)
# or you can specify your own location (in relation to the kernel dir!)
function outitems
{
 outopt=""
 while [ "$outopt" != "0" ]
 do
	clear 
	echo
	echo "   ============[ OUTPUTS MENU ]============="
	echo "   Current: $OUTLOC (see option below)  "
# dont need right now --- RKCRC: $RKCRC"
	echo "   =---------------------------------------="
	echo "   1. zImage to default compile location"
	echo "   2. zImage to KERNEL/MODULES directories"
	echo "   3. zImage to device directories"
	echo "   4. kernel.img to default compile location"
	echo "   5. kernel.img to KERNEL/MODULES dirs"
	echo "   6. kernel.img to device directories"
	echo "   =---------------------------------------="
	if [ "$OUTLOC" = "9" ]; then
	 echo "   9. $OLDESC SELECTED"
	 echo "      DIRECTORY: $KERNPARM2"
	else
	 echo "   9. specify your own type/directory"
	fi
        # echo "   =---------------------------------------="
        # echo "   R. RKCRC it? (for flashing)"
	echo "   ========================================="
	echo "   >> 0. EXIT"
	echo
	read -n 1 -p "Select Option: " outopt
	if [ "$outopt" = "1" ]; then
	   OUTLOC=1
	   KERNPARM1=""
	   KERNPARM2="."
           OLDESC="zImage > default dir"
	   break
	elif [ "$outopt" = "2" ]; then
	   OUTLOC=2
	   KERNPARM1=""
	   KERNPARM2="./NEW"
           OLDESC="zImage > KERNEL/MODULES dirs"
	   break
	elif [ "$outopt" = "3" ]; then
	   OUTLOC=3
	   KERNPARM1=""
	   KERNPARM2="../../../device/rockchip/rk3188/prebuilt/kernel/new"
	   OLDESC="zImage > device dirs"
	   break
	elif [ "$outopt" = "4" ]; then
	   OUTLOC=4
 	   KERNPARM1="kernel.img"
	   KERNPARM2="."
	   OLDESC="kernel.img > default dir"
 	   break
	elif [ "$outopt" = "5" ]; then
	   OUTLOC=5
	   KERNPARM1="kernel.img"
           KERNPARM2="./NEW"
           OLDESC="kernel.img > KERNEL/MODULES dirs"
	   break
	elif [ "$outopt" = "6" ]; then
	   OUTLOC=6
	   KERNPARM1="kernel.img"
	   KERNPARM2="../../../device/rockchip/rk3188/prebuilt/kernel/new"
	   OLDESC="kernel.img > device dirs"
	   break
	elif [ "$outopt" = "9" ]; then
           clear
	   echo
           echo " CUSTOM: What type of kernel to build? "
           echo " ---------------------------------------------- "
           echo " 1. zImage - regular, default, compressed kernel"
           echo " 2. kernel.img - direct to image"
           echo " 3. zkernel.img - compressed direct to image"
           echo " ----------------------------------------------"
           echo " >> 0. Exit the custom option"
           echo
           read -n 1 -p " Kernel type : " KTYPE
           if [ "$KTYPE" = "1" ]; then
            KERNPARM1=""           
            OLDESC="zImage > CUSTOM dirs"
           elif [ "$KTYPE" = "2" ]; then
            KERNPARM1="kernel.img"
	    OLDESC="kernel.img > CUSTOM dirs"
	   elif [ "$KTYPE" = "3" ]; then
	    KERNPARM1="zkernel.img"
	    OLDESC="zkernel.img > CUSTOM dirs"
           else
            break
           fi
           echo
           echo " CUSTOM: Where would you like to dump it?"
           echo " ------------------------------------------------------"
           echo " Specify a location in relation to the kernel directory"            
           echo " (i.e. ../../../kernel will specify 3 directories back"
           echo " and into a directory called kernel, there is no need"
           echo " to specify a trailing backslash)"
           echo
           echo " 1) default (./arch/arm/boot)"
           echo " 2) internal (./NEW)"
           echo " 3) device (../../../device/rockchip/rk3188/prebuilt/kernel/new)"
           echo "   -OR-"
           echo " just enter your custom dir and press enter"
           echo 
           echo " Note: This directory will be created if it doesn't"
           echo " already exist, make sure you have owner permission"
           echo " to write in that directory or it will fail"
           echo " ------------------------------------------------------"
           read -e -p " Directory [1|2|3|enter custom] : " KDIR
           if [ "$KDIR" = "1" ]; then
            KERNPARM2="."
           elif [ "$KDIR" = "2" ]; then
            KERNPARM2="./NEW"
           elif [ "$KDIR" = "3" ]; then
            KERNPARM2="../../../device/rockchip/rk3188/prebuilt/kernel/new"
           else
            KERNPARM2="$KDIR"
           fi
	   OUTLOC=9
         #  rkit
        #elif [[ "$outopt" = "R" || "$outopt" = "r" ]]; then
	#   rkit
	elif [ "$outopt" = "0" ]; then
	   break
	fi
 done
}

# logging options menu, nothing special here
function logitems
{
 logopt=""
 while [ "$logopt" != "0" ]
 do
	clear 
	echo
	echo "   ==============[ LOGS MENU ]=============="
	echo "    Current: $LOGIT (see option below)"
	echo "   =---------------------------------------="
	echo "   1. NO output to BUILD.LOG (only screen)"
	echo "   2. Output to BUILD.LOG & screen (replace)"
	echo "   3. Output to BUILD.LOG & screen (append)"
	echo "   ========================================="
	echo "   >> 0. EXIT"
	echo
	read -n 1 -p "   Select Option: " logopt
	if [ "$logopt" = "1" ]; then
	    LOGIT=1
	    LOGPARMA=""
	    LOGPARM1=""
	    LOGPARMB=""
	    LIDESC="no BUILD.LOG output (only screen)"
	   break
	elif [ "$logopt" = "2" ]; then
	    LOGIT=2
            LOGPARMA="echo '------------------------NEW-KERNEL---------------------' >BUILD.LOG"
	    LOGPARM1="2>&1 | tee -a BUILD.LOG"
            LOGPARMB="echo '------------------------NEW-MODULES--------------------' >>BUILD.LOG"
	   LIDESC="BUILD.LOG/screen output (replace)"
	   break
	elif [ "$logopt" = "3" ]; then
	    LOGIT=3
            LOGPARMA="echo '------------------------NEW-KERNEL---------------------' >>BUILD.LOG"
	    LOGPARM1="2>&1 | tee -a BUILD.LOG"
            LOGPARMB="echo '------------------------NEW-MODULES--------------------' >>BUILD.LOG"
	   LIDESC="BUILD.LOG/screen output (append)"
	   break
        elif [ "$logopt" = "0" ]; then
	   break
	fi
 done
}

# clean-up options menu, nothing fancy here either. Just command to run
# before everything gets started
function cuitems
{
 cuopt=""
 while [ "$cuopt" != "0" ]
 do
	clear 
	echo
	echo "   ============[ CLEAN-UP MENU ]============"
	echo "    Current: $CUTIME (see option below)"
	echo "   =---------------------------------------="
	echo "   1. NO clean-up before Building"
	echo "   2. Prior clean-up (make clean)"
	echo "   3. FULL prior clean-up (make mrproper)"
	echo "   ========================================="
	echo "   >> 0. EXIT"
	echo
	read -n 1 -p "   Select Option: " cuopt
	if [ "$cuopt" = "1" ]; then
	   CUTIME=1
	   CUPARM=""
	   CUDESC="NO clean-up before Building"
	   break
	elif [ "$cuopt" = "2" ]; then
	   CUTIME=2
           CUPARM="make ARCH=arm clean"
	   CUDESC="Prior clean-up (make clean)"
	   break
	elif [ "$cuopt" = "3" ]; then
	   CUTIME=3
           CUPARM="make ARCH=arm mrproper"
	   CUDESC="FULL prior clean-up (make mrproper)"
	   break
        elif [ "$cuopt" = "0" ]; then
	   break
	fi
 done
}

# thread options menu, don't like the default chosen equation? change it
function thrditems
{
 thrdopt=""
 while [ "$thrdopt" != "0" ]
 do
        clear
        echo
        echo "   =============[ THREADS MENU ]============"
        echo "    Current: $THREADS threads"
        echo "   =---------------------------------------="
        echo "   Enter the number of threads to compile"
        echo "   the kernel with. (The general rule is: "
        echo "   # of CPU/cores x 2 = THREADS)"
        echo "   Entering 0 will exit this menu using the"
        echo "   CURRENTLY SET THREADS count above"
        echo "   ========================================="
        echo "   >> 0. Exit"
        echo
        read -e -p "   Select Threads: " thrdopt
        if [ "$thrdopt" = "0" ]; then
           THREADS=$THREADS
	   break           
        else 
           THREADS=$thrdopt
           break
        fi
 done
}

# the actual build process
function buildit
{
 echo
 echo -e "\n-------------------[ CLEANING UP BEFORE BUILD ]-----------------------\n"
 if [ "$CUTIME" = "1" ]; then
  echo "NO clean-up selected, skipping..."
 else
  echo $CUPARM 
  eval $CUPARM
 fi
 echo -e "\n-------------------[ SETTING KERNEL DEFCONFIG ]---------------------\n"
 cp -v ./$DEVICE ./.config
 echo -e "\n-------------------[ BEGINNING KERNEL BUILD ]-----------------------\n"

 eval $LOGPARMA
 NLOG="echo '   =---------------------------------------=' $LOGPARM1"
 eval $NLOG
 NLOG="echo '    TOOLCHAINS: $CROSS_COMPILE' $LOGPARM1"
 eval $NLOG
 NLOG="echo '    DEVICE CONFIG: $DEVICE' $LOGPARM1"
 eval $NLOG
 NLOG="echo '    OUTPUT: $OLDESC' $LOGPARM1"
 eval $NLOG
 NLOG="echo '    LOGS: $LIDESC' $LOGPARM1"
 eval $NLOG
 NLOG="echo '    THEADS: $THREADS' $LOGPARM1"
 eval $NLOG
 NLOG="echo '   =---------------------------------------=' $LOGPARM1"
 eval $NLOG
 echo
 KBLD="make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE -j$THREADS $KERNPARM1 $LOGPARM1"
 echo $KBLD
 eval $KBLD
 echo -e "\n-------------------[ BEGINNING MODULE BUILD ]-----------------------\n"
 eval $LOGPARMB
 MBLD="make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE -j$THREADS modules $LOGPARM1"
 echo $MBLD
 eval $MBLD
 echo -e "\n-------------------[ MOVING STUFF AROUND  ]--------------------------\n"
 if [ "$KERNPARM1" = "" ]; then
  KERNPARM1="./arch/arm/boot/"
  KFILE="zImage"
 else
  KFILE="$KERNPARM1"
  KERNPARM1="./"
 fi
 if [ ! -e $KERNPARM1$KFILE ]; then
  echo
  echo "   [ ERROR ] : NO compiled kernel found!"
  echo "   Check your BUILD.LOG (if applicable)"
  echo "   for probable error(s) during the build."
  echo
  exit
 else
  if [[ "$OUTLOC" = "1" || "$OUTLOC" = "4" ]]; then
   OLOG="echo 'Nothing to move around..' $LOGPARM1"
   eval $OLOG
   echo
  else
   if [ "$KERNPARM2" != "." ]; then
    KCMD="mkdir $KERNPARM2; mkdir $KERNPARM2/modules"
    eval $KCMD
   fi
   OLOG=" echo 'Moving $OLDESC..' $LOGPARM1"
   eval $OLOG
   echo
   OCMD="mv $KERNPARM1/$KFILE $KERNPARM2; find . -name '*.ko' -exec mv {} $KERNPARM2/modules \;"
   eval $OCMD
  fi
  if [ "$RKCRC" = "yes" ]; then
   echo
   echo "RKCRC'ing the kernel (for flashing)..."
   ./rktools/rkcrc -k $KERNPARM2/$KFILE $KERNPARM2/rk-$KFILE
  fi
  echo -e "\n===============================================================\n"
  echo -n "KERNEL: "
  du -ach $KERNPARM2/*
  echo -e "\n===============================================================\n"
  echo "NOTICE: Everything looks good! Happy flashing (your AMP, not people)!"
  echo
  exit
 fi
}

# build overview 
function mihc       
{
 doit=""
 while [[ "$doit" != "n" || "$doit" != "N" || "$doit" != "y" || "$doit" != "Y" ]]
 do
        clear
        echo
	echo "   -------=>  MAKE IT HAP'N CAP'N! <=-------"
	echo "   ========================================="
	echo "   You will be building with the following:"
	echo "   =---------------------------------------="
	echo "    TOOLCHAINS: $CROSS_COMPILE"
	echo "    DEVICE CONFIG: $DEVICE"
	echo "    OUTPUT: $OLDESC"
	echo "    LOGS: $LIDESC"
	echo "    THREADS: $THREADS                   "
# no need for this right now -- RKCRC: $RKCRC"
	echo "   =---------------------------------------="
	echo
        read -n 1 -p "   Proceed with Build? [y|n] : " doit
	if [[ "$doit" = "n" || "$doit" = "N" ]]; then
		break
	elif [[ "$doit" = "y" || "$doit" = "Y" ]]; then
		buildit
	else
		mihc
	fi
 done
}

# saves current build settings to config file (for reuse)
# so you don't have to go through all the options for rebuilds
# (will overwrite any existing build_mk908.cfg file)
function scfig
{
 BCFIG="build_mk908.cfg"
 echo "THREADS=\"$THREADS\"" > $BCFIG
 echo "TOOLCHAIN=\"$TOOLCHAIN\"" >> $BCFIG
 echo "TCDESC=\"$TCDESC\"" >> $BCFIG
 echo "CROSS_COMPILE=\"$CROSS_COMPILE\"" >> $BCFIG
 echo "DEVICE=\"$DEVICE\"" >> $BCFIG
 echo "OUTLOC=\"$OUTLOC\"" >> $BCFIG
 echo "OLDESC=\"$OLDESC\"" >> $BCFIG
 echo "KERNPARM1=\"$KERNPARM1\"" >> $BCFIG
 echo "KERNPARM2=\"$KERNPARM2\"" >> $BCFIG
 echo "LOGIT=\"$LOGIT\"" >> $BCFIG
 echo "LIDESC=\"$LIDESC\"" >> $BCFIG
 echo "LOGPARM1=\"$LOGPARM1\"" >> $BCFIG
 echo "LOGPARMA=\"$LOGPARMA\"" >> $BCFIG
 echo "LOGPARMB=\"$LOGPARMB\"" >> $BCFIG
 echo "CUTIME=\"$CUTIME\"" >> $BCFIG
 echo "CUDESC=\"$CUDESC\"" >> $BCFIG
 echo "CUPARM=\"$CUPARM\"" >> $BCFIG
 echo
 echo
 read -n1 -p "   Config file saved... (press any key to continue)"
}

# fasttrack option
function fasttrack
{
 if [ -s build_mk908.cfg ]; then
  icfig
  buildit
 else
  echo
  echo " ERROR: build_mk908.cfg empty or does not exist!"
  echo "	Configure the build settings and then save it!"
  echo
  break
 fi
}



# main menu
function mainopt
{
 choice=""
 while [ "$choice" != "0" ]
 do
 	clear
 	echo
	echo "   ===================[ BUILD MENU ]=================="
	echo "   1. TOOLCHAIN: $TCDESC"
	echo "   2. CONFIG: $DEVICE"
	echo -n "   3. OUTPUT: $OLDESC"
	if [ "$RKCRC" = "yes" ]; then
	 echo " + RKCRC"
	else
	 echo
	fi
	echo "   4. LOG: $LIDESC"
        echo "   5. CLEAN-UP: $CUDESC"
	echo "   6. THREADS: $THREADS"
	echo "   =-------------------------------------------------="
	echo "   9. BUILD IT! (and they will come...)"
	echo "   =-------------------------------------------------="
	echo "   R. Reload saved build settings (mess up much?)"
        echo "   S. Save current build settings (for reuse)"
        echo "   =============( LABS v$VERS - THESAWOLF )============="
	echo "   >> 0. EXIT"
	echo
	read -n 1 -p "   Enter your choice : " choice
	if [ "$choice" = "1" ]; then
		tcitems
	elif [ "$choice" = "2" ]; then
		cfgitems
	elif [ "$choice" = "3" ]; then
		outitems
	elif [ "$choice" = "4" ]; then
		logitems
	elif [ "$choice" = "5" ]; then
	        cuitems
	elif [ "$choice" = "6" ]; then
		thrditems
	elif [ "$choice" = "9" ]; then
  		mihc
	elif [[ "$choice" = "r" || "$choice" = "R" ]]; then
		icfig
	elif [[ "$choice" = "s" || "$choice" = "S" ]]; then
		scfig
	elif [ "$choice" = "0" ]; then
		outtahere
	fi
 done
}

# commandline parser
while getopts ":hHbBvV?" cmdopt; do
	case $cmdopt in
	[hH?])
		echo	;
		echo "   Lazy-Ass Build Script v$VERS by Thesawolf";
		echo "   ----------------------------------------";
		echo ;
		echo "   -h, -H, -? : This message (duh)";
       		echo ;
		echo "   -b, -B : Unattended build using stored build options";
    	   	echo;
		echo "   -v, -V : Version Info";
		echo ;
		echo "   (what more do you want? sheesh)";
		echo;
		exit;
		;;
	[vV])
		echo;
	    	echo "   Lazy-Ass Builds Script (LABS) v$VERS by Thesawolf";
		echo "   (that's VERSION $VERS, in case you missed it)";
		echo;
		exit;
		;;
	[bB])
	        fasttrack;	
		;;
        *)	
		echo ;
		echo "   ERROR: Invalid parameter!";
		echo;
		exit;
		;;
	esac
done

# functions to run if script run like normal
initdef
icfig
devitems
mainopt


