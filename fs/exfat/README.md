exfat-nofuse
============

<b>LIABILITY NOTICE</b>: I have NOT done extensive testing with this driver
and am giving you (the kernel developer and user) notice that it is NOT
recommended to use this with any important data you do not want to lose or
get corrupted. Until the driver is fully proven as working without issues,
data accessed using this driver is at risk and you agree not to hold this
developer (Thesawolf) liable for any damages that may result, directly or
indirectly, to your usage of this driver whether compiled kernel internal
or as a binary module.

<b>LEGALITY WARNING</b>: This is neither mainline nor GPLv2-strict source. Its LEGAL origins
are questionable and as such, the user should be warned about the legality of
its usage if its incorporated into the kernel or used in its binary form as a
module. It is YOUR responsbility as the kernel developer to notify them and
YOUR acknowledgement of this notice that you will not hold this developer
(Thesawolf) liable for any legal actions that may be taken against you by
any entities (such as Micro$oft, $am$ung, etc.) 

<b>COPYRIGHT CLAIM</b>:
If you an entity that can prove your IP claim to any code contained herein, 
you have but to simply notify me via e-mail (thesawolf[AT]gmail[D0T]com) and 
I will take this source tree down. Please be aware that this source is GPL 
contaminated and as such you are legally AND license-bound by your act of 
GPL usage to make this source availble to the public. 

-----------

Linux non-fuse read/write kernel driver for the exFAT file system.<br />
Originally ported from android kernel v3.0.

To load the driver manually, run this as root:
> modprobe exfat_fs

Free software for the free minds!
