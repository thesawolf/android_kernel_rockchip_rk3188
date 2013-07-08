/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#ifndef _EXFAT_UPCASE_H
#define _EXFAT_UPCASE_H

int exfat_setup_upcase(struct super_block *sb, u32 checksum, u32 clusnr,
		       u64 i_size);
void exfat_free_upcase(struct exfat_sb_info *sbi);
wchar_t exfat_towupper(struct exfat_sb_info *sbi, wchar_t wc);

#endif /* !_EXFAT_UPCASE_H */
