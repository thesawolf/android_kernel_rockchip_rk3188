/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#ifndef _EXFAT_CACHE_H
#define _EXFAT_CACHE_H

#define EXFAT_CACHE_VALID	0	/* special case for valid cache */

struct exfat_cache_id {
	u32	id;			/* cache generation id */
	u32	iclusnr;		/* cluster number on data of inode. */
	u32	clusnr;			/* cluster number on disk. */
	u32	len;			/* number of contiguous clusters */
};

static inline void cache_init(struct exfat_cache_id *cid,
			      u32 iclusnr, u32 clusnr)
{
	cid->id = EXFAT_CACHE_VALID;
	cid->iclusnr = iclusnr;
	cid->clusnr = clusnr;
	cid->len = 1;
}

static inline int cache_contiguous(struct exfat_cache_id *cid, u32 clusnr)
{
	return (cid->clusnr + cid->len) == clusnr;
}

int __init exfat_init_cache_cachep(void);
void exfat_destroy_cache_cachep(void);
int exfat_cache_lookup(struct inode *inode, u32 iclusnr,
		       struct exfat_cache_id *cid);
void exfat_cache_add(struct inode *inode, struct exfat_cache_id *new);
void exfat_cache_inval(struct inode *inode);
void exfat_cache_inode_init_once(struct exfat_inode_info *exi);

#endif /* !_EXFAT_CACHE_H */
