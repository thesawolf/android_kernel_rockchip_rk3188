# Tailoring the script to your particular device's recovery partition's location:
# - Compile rkflashtool by typing in a terminal: make
# - Type in the terminal the command: lsusb
# - Connect one end of the USB cable to your PC
# - Press the reset button using a paperclip, and while pressed, connect the USB cable to the OTG USB port
# - Release the reset button
# - To make sure the above steps were done right, type in a terminal: lsusb
# - You can flash if you see a new device (compare with previous lsusb) with ID: 2207:300a (:310b for RK3188 devices)
# - Run the following (root) command:  sudo ./rkflashtool r 0x0 0x2000 > parm.bin
# - And then this command: cat parm.bin
# - In the output look for "(recovery)" and note well the numbers that precede it. In my case it was: ",0x00008000@0x00010000(recovery),"  (notice the @ character separating the two hex numbers) (0x00010000@0x00012000 (recovery) on my RK3188 MK908)
# - Modify the flash_kernel.sh script's first line ("rkflashtool w 0x... 0x... < recovery.img) to use the two numbers above, BUT SWAPPED (first the hex number after the @, followed by the one before it). In my case that meant leaving the line as:
#       sudo ./rkflashtool w 0x12000 0x10000 < recovery.img
# - Make the script executable by typing in the terminal: chmod +x flash_kernel.sh
./rkcrc -k ../arch/arm/boot/zImage ./rktools/zImage.img

if [ -f zImage.img]; 
	then
		echo "WARNING: You MUST modify this script to flash to the right address or else you WILL BRICK your device."
		read -p "Press Enter if you want to proceed, CTRL-C to exit"
		sudo ./rkflashtool w 0x00004000 0x00006000 < zImage.img
		sudo ./rkflashtool b
	else
		echo "You must compile and generate a valid kernel image to use this tool!"
fi
