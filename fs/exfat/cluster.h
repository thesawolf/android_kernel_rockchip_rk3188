/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#ifndef _EXFAT_CLUSTER_H
#define _EXFAT_CLUSTER_H

struct exfat_clus_map {
	u32	iclusnr;		/* cluster number on data of inode. */
	u32	clusnr;			/* cluster number on disk. */
	u32	len;			/* number of contiguous clusters */
};

int exfat_get_cluster(struct inode *inode, u32 iclusnr, u32 clus_len,
		      struct exfat_clus_map *cmap);

#endif /* !_EXFAT_CLUSTER_H */
