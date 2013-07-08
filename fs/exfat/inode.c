/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#include <linux/buffer_head.h>
#include <linux/mpage.h>
#include <linux/hash.h>
#include "exfat.h"
#include "cluster.h"

int exfat_get_block(struct inode *inode, sector_t iblock,
		    struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct exfat_inode_info *exi = EXFAT_I(inode);
	unsigned long blocksize, max_blocks, mapped_blocks;
	sector_t blocknr, last_iblock;
	u32 iclusnr, last_iclusnr;
	int err, offset;

	blocksize = 1 << inode->i_blkbits;;
	last_iblock = (i_size_read(inode) + (blocksize - 1))
		>> inode->i_blkbits;
	if (!create && iblock >= last_iblock)
		return -EIO;

	/*
	 * Find the available blocks already allocating
	 */

	last_iclusnr = (last_iblock + (sbi->bpc - 1)) >> sbi->bpc_bits;
	iclusnr = iblock >> sbi->bpc_bits;
	offset = iblock & (sbi->bpc - 1);

	max_blocks = bh_result->b_size >> inode->i_blkbits;
	mapped_blocks = 0;
	if (iclusnr < last_iclusnr) {
		/* Some clusters are available */
		struct exfat_clus_map cmap;
		u32 clus_len;

		clus_len = (max_blocks + (sbi->bpc - 1)) >> sbi->bpc_bits;
		err = exfat_get_cluster(inode, iclusnr, clus_len, &cmap);
		if (err)
			return err;

		if (!exfat_ent_valid(sbi, cmap.clusnr)) {
			printk(KERN_ERR "exFAT: unexpected FAT entry"
			       " (start cluster 0x%08x, entry 0x%08x)\n",
			       exi->clusnr, cmap.clusnr);
			return -EIO;
		}

		blocknr = exfat_clus_to_blknr(sbi, cmap.clusnr) + offset;
		mapped_blocks = (cmap.len << sbi->bpc_bits) - offset;
		if (iblock < last_iblock) {
			/* Some blocks are available */
			unsigned long avail_blocks = last_iblock - iblock;

			mapped_blocks = min(mapped_blocks, avail_blocks);
			mapped_blocks = min(mapped_blocks, max_blocks);

			map_bh(bh_result, sb, blocknr);
			bh_result->b_size = mapped_blocks << inode->i_blkbits;
			return 0;
		}
		/* We can allocate blocks in this last cluster */
	}
	/* FIXME: write is not supported */
	DEBUG0_ON(create);

	/*
	 * Allocate new blocks
	 */
	DEBUG_ON(iblock != exi->phys_size >> inode->i_blkbits,
		 "iblock %llu, phys_size %lld\n", (llu)iblock, exi->phys_size);
	DEBUG_ON(mapped_blocks >= sbi->bpc,
		 "mapped_blocks %lu\n", mapped_blocks);

	return -EIO;

	/* FIXME: multiple cluster allocation would be desirable. */
	if (!mapped_blocks) {
		/* We have to allocate new cluster */
//		err = fat_add_cluster(inode, iclusnr);
		if (err)
			return err;

		mapped_blocks += sbi->bpc;
	} else if (mapped_blocks < max_blocks) {
		/* Try to allocate next cluster of this cluster */
//		err = fat_add_next_cluster(inode, iclusnr);
		if (err)
			return err;

		mapped_blocks += sbi->bpc;
	}

	mapped_blocks = min(mapped_blocks, max_blocks);
	exi->phys_size += mapped_blocks << inode->i_blkbits;

	set_buffer_new(bh_result);
	map_bh(bh_result, sb, blocknr);
	bh_result->b_size = mapped_blocks << inode->i_blkbits;

	return 0;
}

static int exfat_readpage(struct file *file, struct page *page)
{
	return mpage_readpage(page, exfat_get_block);
}

static int exfat_readpages(struct file *file, struct address_space *mapping,
			   struct list_head *pages, unsigned nr_pages)
{
	return mpage_readpages(mapping, pages, nr_pages, exfat_get_block);
}

static sector_t exfat_bmap(struct address_space *mapping, sector_t iblock)
{
	sector_t blocknr;

	/* exfat_get_cluster() assumes the requested blocknr isn't truncated. */
	mutex_lock(&mapping->host->i_mutex);
	blocknr = generic_block_bmap(mapping, iblock, exfat_get_block);
	mutex_unlock(&mapping->host->i_mutex);

	return blocknr;
}

static const struct address_space_operations exfat_aops = {
	.readpage		= exfat_readpage,
	.readpages		= exfat_readpages,
//	.writepage		= ext4_da_writepage,
//	.writepages		= ext4_da_writepages,
//	.sync_page		= block_sync_page,
//	.write_begin		= ext4_da_write_begin,
//	.write_end		= ext4_da_write_end,
	.bmap			= exfat_bmap,
//	.invalidatepage		= ext4_da_invalidatepage,
//	.releasepage		= ext4_releasepage,
//	.direct_IO		= ext4_direct_IO,
//	.migratepage		= buffer_migrate_page,
//	.is_partially_uptodate	= block_is_partially_uptodate,
};

int exfat_getattr(struct vfsmount *mnt, struct dentry *dentry,
		  struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;
	generic_fillattr(inode, stat);
	stat->blksize = EXFAT_SB(inode->i_sb)->clus_size;
	return 0;
}

static const struct file_operations exfat_file_ops = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
//	.unlocked_ioctl	= fat_generic_ioctl,
#ifdef CONFIG_COMPAT
//	.compat_ioctl	= fat_compat_dir_ioctl,
#endif
	.mmap		= generic_file_mmap,
	.open		= generic_file_open,
//	.fsync		= file_fsync,
	.splice_read	= generic_file_splice_read,
	.splice_write	= generic_file_splice_write,
};

static const struct inode_operations exfat_file_inode_ops = {
//	.truncate	= ext4_truncate,
//	.permission	= ext4_permission,
//	.setattr	= ext4_setattr,
	.getattr	= exfat_getattr
#ifdef CONFIG_EXT4DEV_FS_XATTR
//	.setxattr	= generic_setxattr,
//	.getxattr	= generic_getxattr,
//	.listxattr	= ext4_listxattr,
//	.removexattr	= generic_removexattr,
#endif
//	.fallocate	= ext4_fallocate,
//	.fiemap		= ext4_fiemap,
};

/* See comment in fs/fat/inode.c */
/* FIXME: rethink those hash stuff */
void exfat_hash_init(struct super_block *sb)
{
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	int i;

	spin_lock_init(&sbi->inode_hash_lock);
	for (i = 0; i < EXFAT_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&sbi->inode_hashtable[i]);
}

static inline u32 exfat_hash(struct super_block *sb, sector_t blocknr,
			     unsigned long offset)
{
	u32 key = (((loff_t)blocknr << sb->s_blocksize_bits) | offset)
		>> EXFAT_CHUNK_BITS;
	return hash_32(key, EXFAT_HASH_BITS);
}

static void exfat_attach(struct inode *inode, struct exfat_parse_data *pd)
{
	struct super_block *sb = inode->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct exfat_inode_info *exi = EXFAT_I(inode);
	u32 hashval = exfat_hash(sb, pd->bhs[0]->b_blocknr, pd->bh_offset);
	struct hlist_head *head = sbi->inode_hashtable + hashval;
	int i;

	spin_lock(&sbi->inode_hash_lock);
	for (i = 0; i < pd->nr_bhs; i++)
		exi->de_blocknr[i] = pd->bhs[i]->b_blocknr;
	exi->de_offset = pd->bh_offset;
	exi->de_size = pd->size;
	hlist_add_head(&exi->i_hash, head);
	spin_unlock(&sbi->inode_hash_lock);
}

static void __exfat_detach(struct exfat_inode_info *exi)
{
	exi->de_blocknr[0] = 0;
	exi->de_offset = -1;
	exi->de_size = -1;
}

void exfat_detach(struct inode *inode)
{
	struct exfat_sb_info *sbi = EXFAT_SB(inode->i_sb);
	struct exfat_inode_info *exi = EXFAT_I(inode);

	spin_lock(&sbi->inode_hash_lock);
	__exfat_detach(exi);
	hlist_del_init(&exi->i_hash);
	spin_unlock(&sbi->inode_hash_lock);
}

struct inode *exfat_ilookup(struct super_block *sb, sector_t blocknr,
			    unsigned long offset)
{
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	u32 hashval = exfat_hash(sb, blocknr, offset);
	struct hlist_head *head = sbi->inode_hashtable + hashval;
	struct hlist_node *_p;
	struct exfat_inode_info *exi;
	struct inode *inode = NULL;

	spin_lock(&sbi->inode_hash_lock);
	hlist_for_each_entry(exi, _p, head, i_hash) {
		DEBUG_ON(exi->vfs_inode.i_sb != sb,
			 "vfs.sb %p, sb %p\n", exi->vfs_inode.i_sb, sb);
		if (exi->de_blocknr[0] != blocknr)
			continue;
		if (exi->de_offset != offset)
			continue;
		inode = igrab(&exi->vfs_inode);
		if (inode)
			break;
	}
	spin_unlock(&sbi->inode_hash_lock);
	return inode;
}

static void exfat_fill_inode(struct inode *inode, unsigned long ino,
			     struct exfat_chunk_dirent *dirent,
			     struct exfat_chunk_data *data)
{
	struct exfat_sb_info *sbi = EXFAT_SB(inode->i_sb);
	struct exfat_inode_info *exi = EXFAT_I(inode);

	inode->i_ino = ino;
	inode->i_nlink = 1;	/*  Windows use 1 even if directory.*/
	inode->i_uid = sbi->opts.uid;
	inode->i_gid = sbi->opts.gid;
	inode->i_version = 1;
	/* FIXME: inode->i_size is "signed", have to check overflow? */
	inode->i_size = le64_to_cpu(data->size);
	/* FIXME: sane time conversion is needed */
	exfat_time_exfat2unix(&inode->i_mtime, dirent->mtime, dirent->mdate,
			      dirent->mtime_cs);
	exfat_time_exfat2unix(&inode->i_atime, dirent->atime, dirent->adate, 0);
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;
	inode->i_blocks = (inode->i_size + (sbi->clus_size - 1)) >> 9;
/*	inode->i_generation = get_seconds(); FIXME: don't support nfs? */
	if (dirent->attrib & cpu_to_le16(EXFAT_ATTR_DIR)) {
		inode->i_mode = S_IFDIR | sbi->opts.dmode;
		inode->i_op = &exfat_dir_inode_ops;
		inode->i_fop = &exfat_dir_ops;
	} else {
		inode->i_mode = S_IFREG | sbi->opts.fmode;
		inode->i_op = &exfat_file_inode_ops;
		inode->i_fop = &exfat_file_ops;
		inode->i_mapping->a_ops = &exfat_aops;
	}
#if 0
	/* FIXME: separate S_NOCMTIME to S_NOCTIME and S_NOMTIME,
	 * then exFAT/FAT can use S_NOCTIME. */
	inode->i_flags |= S_NOCTIME;

	/* FIXME: how handle these attrib? */
	if (dirent->attrib & cpu_to_le16(EXFAT_ATTR_SYSTEM)) {
		if (sbi->opts.sys_immutable)
			inode->i_flags |= S_IMMUTABLE;
	}
	if (dirent->attrib & cpu_to_le16(EXFAT_ATTR_RO)) {
		if (sbi->opts.sys_immutable)
			inode->i_flags |= S_IMMUTABLE;
	}
#endif
	exi->phys_size = inode->i_size;
	exi->clusnr = le32_to_cpu(data->clusnr);
	exi->attrib = dirent->attrib;
	exi->data_flag = data->flag;

	__exfat_detach(exi);
}

struct inode *exfat_iget(struct super_block *sb,
			 struct exfat_parse_data *pd,
			 struct exfat_chunk_dirent *dirent,
			 struct exfat_chunk_data *data)
{
	struct inode *inode;
	unsigned long ino;

	/* FIXME: this can use ilookup5 (with inode_lock). Which one is fast? */
	inode = exfat_ilookup(sb, pd->bhs[0]->b_blocknr, pd->bh_offset);
	if (inode)
		return inode;

	inode = new_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	ino = iunique(sb, EXFAT_RESERVED_INO);
	exfat_fill_inode(inode, ino, dirent, data);

	exfat_attach(inode, pd);
	insert_inode_hash(inode);

	return inode;
}

struct inode *exfat_new_internal_inode(struct super_block *sb,
				       unsigned long ino, u16 attrib,
				       u32 clusnr, u64 size)
{
	struct inode *inode;
	struct exfat_chunk_dirent dirent = {
		.attrib	= cpu_to_le16(attrib),
	};
	struct exfat_chunk_data data = {
		.flag	= EXFAT_DATA_NORMAL,
		.clusnr	= cpu_to_le32(clusnr),
		.size	= cpu_to_le64(size),
	};

	inode = new_inode(sb);
	if (inode) {
		exfat_fill_inode(inode, ino, &dirent, &data);
		/* The internal inode doesn't have timestamp */
		inode->i_flags |= S_NOATIME | S_NOCMTIME;
	}
	return inode;
}

struct inode *exfat_rootdir_iget(struct super_block *sb)
{
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct inode *inode;
	struct exfat_clus_map cmap;
	int err;

	err = -ENOMEM;
	inode = exfat_new_internal_inode(sb, EXFAT_ROOT_INO, EXFAT_ATTR_DIR,
					 sbi->rootdir_clusnr, 0);
	if (!inode)
		goto error;

	/* Get last iclusnr to set ->i_size */
	err = exfat_get_cluster(inode, EXFAT_ENT_EOF, 0, &cmap);
	if (err)
		goto error_iput;

	if (!exfat_ent_eof(sbi, cmap.clusnr)) {
		printk(KERN_ERR "exFAT: found invalid FAT entry 0x%08x"
		       " for root directory\n", cmap.clusnr);
		err = -EIO;
		goto error_iput;
	}

	inode->i_size = cmap.iclusnr << sbi->clus_bits;
	EXFAT_I(inode)->phys_size = inode->i_size;
	inode->i_blocks = (inode->i_size + (sbi->clus_size - 1)) >> 9;

	insert_inode_hash(inode);

	return inode;

error_iput:
	iput(inode);
error:
	return ERR_PTR(err);
}
