/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#ifndef _EXFAT_BITMAP_H
#define _EXFAT_BITMAP_H

int exfat_setup_bitmap(struct super_block *sb, u32 clusnr, u64 i_size);
void exfat_free_bitmap(struct exfat_sb_info *sbi);

#endif /* !_EXFAT_BITMAP_H */
