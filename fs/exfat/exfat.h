/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#ifndef _EXFAT_H
#define _EXFAT_H

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/nls.h>
#include "exfat_fs.h"

/* FIXME: remove this */
#define DEBUG0_ON(cond, format...)	do {		\
	if (cond) {					\
		printk("---- DEBUG_ON: %s:%u -----\n",	\
		       __func__, __LINE__);		\
		BUG();					\
	}						\
} while (0)
#define DEBUG_ON(cond, format...)	do {		\
	if (cond) {					\
		printk("---- DEBUG_ON: %s:%u -----\n",	\
		       __func__, __LINE__);		\
		printk(format);				\
		BUG();					\
	}						\
} while (0)

#ifdef CONFIG_EXFAT_FS_DEBUG
#define EXFAT_DEBUG(x...)	printk(x)
#else //CONFIG_EXFAT_FS_DEBUG
#define EXFAT_DEBUG(x...)
#endif

/* Current limitations */
#define EXFAT_MIN_BLKSIZE	(1 << EXFAT_MIN_BLKBITS)
#define EXFAT_MIN_BLKBITS	9			/* 512 */
#define EXFAT_MAX_BLKBITS	12			/* 4096 */
#define EXFAT_MAX_CLUS_BITS	25			/* 32MB */

/* Number of buffers for maximum size of a directory entry */
#define EXFAT_DE_MAX_SUB_CHUNKS	255
#define EXFAT_DE_MAX_CHUNKS	(1 + EXFAT_DE_MAX_SUB_CHUNKS)
#define EXFAT_DE_MAX_SIZE	(EXFAT_DE_MAX_CHUNKS << EXFAT_CHUNK_BITS)
#define EXFAT_DE_MAX_BH \
	(DIV_ROUND_UP(EXFAT_DE_MAX_SIZE, EXFAT_MIN_BLKSIZE) + 1)

/* Reverved inode numbers */
#define EXFAT_ROOT_INO		1
#define EXFAT_BITMAP_INO	2
#define EXFAT_UPCASE_INO	3
#define EXFAT_RESERVED_INO	EXFAT_UPCASE_INO

#define EXFAT_HASH_BITS		8
#define EXFAT_HASH_SIZE		(1UL << EXFAT_HASH_BITS)

struct upcase;

struct exfat_mount_opts {
	uid_t		uid;
	gid_t		gid;
	umode_t		fmode;
	umode_t		dmode;
	struct nls_table *nls;
};

struct exfat_sb_info {
	spinlock_t	inode_hash_lock;
	struct hlist_head inode_hashtable[EXFAT_HASH_SIZE];

	sector_t	fat_blocknr;		/* start blocknr of FAT */
	u32		fat_block_counts;	/* number of FAT blocks */
	u32		fpb;			/* FAT-entries per block */
	u8		fpb_bits;		/* bits of FAT-entries per
						 * block */
	u8		clus_bits;		/* bits of cluster size */
	u8		bpc_bits;		/* bits of block per cluster */
	unsigned int	bpc;			/* block per cluster */
	unsigned long	clus_size;		/* cluster size */

	sector_t	clus_blocknr;		/* start blocknr of cluster */
	u32		rootdir_clusnr;		/* start clusnr of rootdir */
	u32		total_clusters;		/* number of total clusters */
	u32		free_clusters;		/* number of free clusters */

	u16		state;			/* state of this volume */
	u32		serial_number;		/* serial number */

	struct inode	*bitmap_inode;		/* inode for bitmap */
	struct upcase	*upcase;		/* upper-case table info */

	struct exfat_mount_opts opts;
};

struct exfat_inode_info {
	struct hlist_node i_hash;

	spinlock_t	cache_lru_lock;		/* lock for caches */
	struct list_head cache_lru;		/* LRU list for caches */
	int		nr_caches;		/* number of caches */
	u32		cache_valid_id;		/* For detecting the race of
						 * truncate and get_cluster */

	/* NOTE: phys_size is 64bits, so must hold ->i_mutex to access */
	loff_t		phys_size;		/* physically allocated size */

	u32		clusnr;
	__le16		attrib;
	u8		data_flag;

	/* FIXME: de_blocknr is big. should rethink more small, and more
	 * faster way */
	sector_t	de_blocknr[EXFAT_DE_MAX_BH];/* blocknr of this entry */
	unsigned long	de_offset;		/* start offset in buffer */
	unsigned long	de_size;		/* size of this entry */

	struct inode	vfs_inode;
};

struct exfat_parse_data {
	unsigned long	size;			/* size of this entry */

	struct buffer_head *bhs[EXFAT_DE_MAX_BH];/* buffers for this entry */
	unsigned long	bh_offset;		/* start offset in buffer */
	int		nr_bhs;			/* number of buffer heads */
#ifdef DEBUG
	int		bh_cnt;
#endif
};

static inline struct exfat_sb_info *EXFAT_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct exfat_inode_info *EXFAT_I(struct inode *inode)
{
	return container_of(inode, struct exfat_inode_info, vfs_inode);
}

static inline sector_t exfat_clus_to_blknr(struct exfat_sb_info *sbi,
					   u32 clusnr)
{
	DEBUG_ON(clusnr < EXFAT_START_ENT, "clusnr 0x%08x\n", clusnr);
	return sbi->clus_blocknr +
		((sector_t)(clusnr - EXFAT_START_ENT) << sbi->bpc_bits);
}

/* Is the FAT entry free? */
static inline int exfat_ent_free(struct exfat_sb_info *sbi, u32 clusnr)
{
	return clusnr == EXFAT_ENT_FREE;
}

/* Is the FAT entry bad cluster? */
static inline int exfat_ent_bad(struct exfat_sb_info *sbi, u32 clusnr)
{
	return clusnr == EXFAT_ENT_BAD;
}

/* Is the FAT entry EOF? */
static inline int exfat_ent_eof(struct exfat_sb_info *sbi, u32 clusnr)
{
	return clusnr == EXFAT_ENT_EOF;
}

/* Is the FAT entry valid as normal entry? */
static inline int exfat_ent_valid(struct exfat_sb_info *sbi, u32 clusnr)
{
	return (clusnr - EXFAT_START_ENT) < sbi->total_clusters;
}

/* helper for printk */
typedef unsigned long long	llu;

/* dir.c */
extern struct dentry_operations exfat_dentry_ops;
int exfat_read_rootdir(struct inode *inode);
extern const struct inode_operations exfat_dir_inode_ops;
extern const struct file_operations exfat_dir_ops;

/* inode.c */
int exfat_get_block(struct inode *inode, sector_t iblock,
		    struct buffer_head *bh_result, int create);
int exfat_getattr(struct vfsmount *mnt, struct dentry *dentry,
		  struct kstat *stat);
void exfat_hash_init(struct super_block *sb);
void exfat_detach(struct inode *inode);
struct inode *exfat_ilookup(struct super_block *sb, sector_t blocknr,
			    unsigned long offset);
struct inode *exfat_iget(struct super_block *sb,
			 struct exfat_parse_data *pd,
			 struct exfat_chunk_dirent *dirent,
			 struct exfat_chunk_data *data);
struct inode *exfat_new_internal_inode(struct super_block *sb,
				       unsigned long ino, u16 attrib,
				       u32 clusnr, u64 size);
struct inode *exfat_rootdir_iget(struct super_block *sb);

/* utils.c */
u32 exfat_checksum32(u32 sum, const void *__buf, int size);
u16 exfat_checksum16(u16 sum, const void *__buf, int size);
int exfat_conv_u2c(struct nls_table *nls,
		   const __le16 **uni, unsigned int *uni_left,
		   unsigned char **out, unsigned int *out_left);
int exfat_conv_c2u(struct nls_table *nls,
		   const unsigned char **in, unsigned int *in_left,
		   wchar_t **uni, unsigned int *uni_left);
void exfat_time_exfat2unix(struct timespec *ts, __le16 __time, __le16 __date,
			   u8 time_cs);
void exfat_time_unix2exfat(struct timespec *ts, __le16 *time, __le16 *date,
			   u8 *time_cs);
void exfat_fs_panic(struct super_block *sb, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3))) __cold;

#endif /* !_EXFAT_H */
