/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/magic.h>
#include <linux/buffer_head.h>
#include <linux/mount.h>
#include <linux/statfs.h>
#include <linux/seq_file.h>
#include <linux/parser.h>
#include "exfat.h"
#include "cache.h"
#include "bitmap.h"
#include "upcase.h"

static struct kmem_cache *exfat_inode_cachep;

static void exfat_inode_init_once(void *obj)
{
	struct exfat_inode_info *exi = obj;

	INIT_HLIST_NODE(&exi->i_hash);
	exfat_cache_inode_init_once(exi);
	inode_init_once(&exi->vfs_inode);
}

static int exfat_init_inodecache(void)
{
	exfat_inode_cachep = kmem_cache_create("exfat_inode_cache",
					       sizeof(struct exfat_inode_info),
					       0, (SLAB_RECLAIM_ACCOUNT |
						   SLAB_MEM_SPREAD),
					       exfat_inode_init_once);
	if (!exfat_inode_cachep)
		return -ENOMEM;
	return 0;
}

static void exfat_destroy_inodecache(void)
{
	kmem_cache_destroy(exfat_inode_cachep);
}

static struct inode *exfat_alloc_inode(struct super_block *sb)
{
	struct exfat_inode_info *exi;
	exi = kmem_cache_alloc(exfat_inode_cachep, GFP_KERNEL);
	if (!exi)
		return NULL;
	return &exi->vfs_inode;
}

static void exfat_destroy_inode(struct inode *inode)
{
	kmem_cache_free(exfat_inode_cachep, EXFAT_I(inode));
}

static void exfat_clear_inode(struct inode *inode)
{
	exfat_cache_inval(inode);
	exfat_detach(inode);
}

enum { Opt_uid, Opt_gid, Opt_fmode, Opt_dmode, Opt_nls, Opt_err, };

static match_table_t exfat_tokens = {
	{ Opt_uid, "uid=%u" },
	{ Opt_gid, "gid=%u" },
	{ Opt_fmode, "fmode=%o" },
	{ Opt_dmode, "dmode=%o" },
	/* FIXME: should use "iocharset"? */
	{ Opt_nls, "nls=%s" },
	{ Opt_err, NULL },
};

static int exfat_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	struct exfat_sb_info *sbi = EXFAT_SB(mnt->mnt_sb);
	struct exfat_mount_opts *opts = &sbi->opts;

	if (opts->uid)
		seq_printf(m, ",uid=%u", opts->uid);
	if (opts->gid)
		seq_printf(m, ",gid=%u", opts->gid);
	seq_printf(m, ",fmode=%04o", opts->fmode);
	seq_printf(m, ",dmode=%04o", opts->dmode);
	seq_printf(m, ",nls=%s", opts->nls->charset);

	return 0;
}

static int parse_options(struct exfat_mount_opts *opts, char *data, int silent,
			 int is_remount)
{
	char *p, *nls_str;
	substring_t args[MAX_OPT_ARGS];
	int err, option;

	opts->uid	= 0;
	opts->gid	= 0;
	opts->fmode	= S_IWUSR | S_IRUGO;
	opts->dmode	= S_IWUSR | S_IRUGO | S_IXUGO;
	opts->nls	= load_nls_default();

	if (!data)
		goto out;

	err = -EINVAL;
	while ((p = strsep(&data, ",")) != NULL) {
		int token;
		if (!*p)
			continue;

		token = match_token(p, exfat_tokens, args);
		switch (token) {
		case Opt_uid:
			if (is_remount) {
				if (!silent)
					printk(KERN_ERR "exFAT: cannot change"
					       " uid option on remount\n");
				goto error;
			}
			if (match_int(&args[0], &option))
				goto error;
			opts->uid = option;
			break;
		case Opt_gid:
			if (is_remount) {
				if (!silent)
					printk(KERN_ERR "exFAT: cannot change"
					       " gid option on remount\n");
				goto error;
			}
			if (match_int(&args[0], &option))
				goto error;
			opts->gid = option;
			break;
		case Opt_fmode:
			if (is_remount) {
				if (!silent)
					printk(KERN_ERR "exFAT: cannot change"
					       " fmode option on remount\n");
				goto error;
			}
			if (match_octal(&args[0], &option))
				goto error;
			opts->fmode = option & S_IRWXUGO;
			break;
		case Opt_dmode:
			if (is_remount) {
				if (!silent)
					printk(KERN_ERR "exFAT: cannot change"
					       " dmode option on remount\n");
				goto error;
			}
			if (match_octal(&args[0], &option))
				goto error;
			opts->dmode = option & S_IRWXUGO;
			break;
		case Opt_nls:
			if (is_remount) {
				if (!silent)
					printk(KERN_ERR "exFAT: cannot change"
					       " nls option on remount\n");
				goto error;
			}
			nls_str = match_strdup(&args[0]);
			if (!nls_str) {
				err = -ENOMEM;
				goto error;
			}

			if (opts->nls)
				unload_nls(opts->nls);
			opts->nls = load_nls(nls_str);
			if (!opts->nls) {
				if (!silent)
					printk(KERN_ERR "exFAT: couldn't load "
					       " nls \"%s\"\n", nls_str);
			}
			kfree(nls_str);
			if (!opts->nls)
				goto error;
			break;
		/* Unknown option */
		default:
			if (!silent) {
				printk(KERN_ERR "exFAT: Unrecognized mount"
				       " option \"%s\" or missing value\n", p);
			}
			goto error;
		}
	}

out:
	return 0;

error:
	if (opts->nls) {
		unload_nls(opts->nls);
		opts->nls = NULL;
	}
	return err;
}

static int exfat_remount(struct super_block *sb, int *flags, char *data)
{
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct exfat_mount_opts opts;
	int err, remount_rw;

	remount_rw = !(*flags & MS_RDONLY) && (sb->s_flags & MS_RDONLY);

	if (!sbi->upcase && remount_rw) {
		printk(KERN_ERR "exFAT: upper-case table isn't available,"
		       " so filesystem can't make writable\n");
		return -EINVAL;
	}

	/*
	 * FIXME: currently, can't change all options on remount, must
	 * rethink about remount options.
	 */
	err = parse_options(&opts, data, 0, 1);
	if (err)
		return err;

	return 0;
}

static void exfat_put_super(struct super_block *sb)
{
	struct exfat_sb_info *sbi = EXFAT_SB(sb);

	exfat_free_bitmap(sbi);
	exfat_free_upcase(sbi);
	if (sbi->opts.nls)
		unload_nls(sbi->opts.nls);
	sb->s_fs_info = NULL;
	kfree(sbi);
}

static int exfat_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct exfat_sb_info *sbi = EXFAT_SB(dentry->d_sb);

	buf->f_type = EXFAT_SUPER_MAGIC;
	buf->f_bsize = sbi->clus_size;
	buf->f_blocks = sbi->total_clusters;
	buf->f_bfree = sbi->free_clusters;
	buf->f_bavail = sbi->free_clusters;
#if 0
	buf->f_files = buf->f_blocks << (sbi->clus_bits - EXFAT_CHUNK_BITS) / 3;
	buf->f_ffree = buf->f_blocks << (sbi->clus_bits - EXFAT_CHUNK_BITS) / 3;
#endif
	buf->f_fsid.val[0] = sbi->serial_number;
	/*buf->f_fsid.val[1];*/
	buf->f_namelen = EXFAT_MAX_NAMELEN;
	buf->f_frsize = sbi->clus_size;

	return 0;
}

static const struct super_operations exfat_sops = {
	.alloc_inode	= exfat_alloc_inode,
	.destroy_inode	= exfat_destroy_inode,
//	.write_inode	= ext4_write_inode,
//	.delete_inode	= ext4_delete_inode,
	.clear_inode	= exfat_clear_inode,
	.put_super	= exfat_put_super,
//	.write_super	= ext4_write_super,
	.statfs		= exfat_statfs,
	.remount_fs	= exfat_remount,
	.show_options	= exfat_show_options,
#ifdef CONFIG_QUOTA
//	.quota_read	= ext2_quota_read,
//	.quota_write	= ext2_quota_write,
#endif
};

static int exfat_exsb_validate(struct super_block *sb,
			       struct exfat_super_block *exsb,
			       int silent)
{
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
#if 0
	struct inode *bd_inode = sb->s_bdev->bd_inode;
#endif
	const char *message;
	sector_t nr_sectors;
	int i, zero;

	/*
	 * FIXME: should check the physical block aligment? (eraceblock too?)
	 * If aligment is invalid, should be warn it (performance may be bad)
	 */

	if (memcmp(exsb->oem_id, EXFAT_OEM_ID, sizeof(exsb->oem_id))) {
		message = "invalid OEM ID";
		goto error;
	}

	zero = 1;
	for (i = 0; i < ARRAY_SIZE(exsb->unused1); i++) {
		if (exsb->unused1[i]) {
			zero = 0;
			break;
		}
	}
	if (!zero || exsb->unused0) {
		message = "invalid garbage found in boot sector";
		goto error;
	}

	if (exsb->blocksize_bits < EXFAT_MIN_BLKBITS ||
	    EXFAT_MAX_BLKBITS < exsb->blocksize_bits) {
		message = "invalid blocksize";
		goto error;
	}
	if ((exsb->blocksize_bits + sbi->bpc_bits) > EXFAT_MAX_CLUS_BITS) {
		message = "cluster size is too large";
		goto error;
	}
#if 0
	if ((i_size_read(bd_inode) >> exsb->blocksize_bits) < EXFAT_SB_BLOCKS) {
		message = "too small device";
		goto error;
	}
#endif
	nr_sectors = le64_to_cpu(exsb->nr_sectors);
	if (!nr_sectors) {
		message = "invalid total sectors count";
		goto error;
	}
	if (sbi->fat_blocknr < EXFAT_SB_BLOCKS) {
		message = "invalid block number of FAT";
		goto error;
	}
	if (!sbi->fat_block_counts) {
		message = "invalid FAT blocks count";
		goto error;
	}
	if (sbi->clus_blocknr < EXFAT_SB_BLOCKS) {
		message = "invalid block number of start cluster";
		goto error;
	}
	if (!sbi->total_clusters ||
	    (sbi->total_clusters << sbi->bpc_bits) > nr_sectors) {
		message = "invalid total clusters count";
		goto error;
	}
	if (sbi->rootdir_clusnr < EXFAT_START_ENT) {
		message = "invalid block number of root directory";
		goto error;
	}
#if 0
	if (exsb->xxxx02 != 0 && exsb->xxxx02 != 1) {
		message = "invalid xxxx02";
		goto error;
	}
	if (exsb->xxxx03 != 0 && exsb->xxxx03 != 1) {
		message = "invalid xxxx03";
		goto error;
	}
#endif
	if (exsb->signature != cpu_to_le16(BOOT_SIG)) {
		message = "invalid boot block signature";
		goto error;
	}

	return 0;

error:
	if (!silent)
		printk(KERN_INFO "exFAT: %s\n", message);

	return -EINVAL;
}

/* Verify checksum for super block */
static int exfat_exsb_verify_checksum(struct super_block *sb, int silent)
{
	int i;
#if 0
	struct address_space *bd_mapping = sb->s_bdev->bd_inode->i_mapping;
	struct file_ra_state ra;
	int nr_pages;

	memset(&ra, 0, sizeof(ra));
	file_ra_state_init(&ra, bd_mapping);

	/* FIXME: sector 0 was already read, so 0 page will not be submitted */
	nr_pages = (EXFAT_SB_BLOCKS * 2)
		>> (PAGE_CACHE_SHIFT - sb->s_blocksize_bits);
	ra.ra_pages = nr_pages;
	page_cache_sync_readahead(bd_mapping, &ra, NULL, 0, nr_pages);
#endif
	for (i = 0; i < 2; i++) {
		sector_t start_blocknr = EXFAT_SB_BLOCKS * i;
		struct buffer_head *bh;
		__le32 *checksums;
		u32 s;
		unsigned int n, blocknr;

		bh = sb_bread(sb, start_blocknr);
		if (!bh)
			goto error_io;
		/* Skip exsb->state and exsb->allocated_percent */
		s = exfat_checksum32(0, bh->b_data, 0x6a);
		s = exfat_checksum32(s, bh->b_data + 0x6c, 4);
		s = exfat_checksum32(s, bh->b_data + 0x71, bh->b_size - 0x71);
		brelse(bh);
		for (blocknr = 1; blocknr < EXFAT_SB_DATA_BLOCKS; blocknr++) {
			bh = sb_bread(sb, start_blocknr + blocknr);
			if (!bh)
				goto error_io;
			s = exfat_checksum32(s, bh->b_data, bh->b_size);
			brelse(bh);
		}

		bh = sb_bread(sb, start_blocknr + EXFAT_SB_CKSUM_BLOCK);
		if (!bh)
			goto error_io;
		checksums = (__le32 *)bh->b_data;
		for (n = 0; n < (bh->b_size / sizeof(checksums[0])); n++) {
			if (checksums[n] != cpu_to_le32(s)) {
				brelse(bh);
				goto error_checksum;
			}
		}
		brelse(bh);
	}

	return 0;

error_io:
	printk(KERN_ERR "exFAT: couldn't read super block to verify\n");
	return -EIO;

error_checksum:
	if (!silent)
		printk(KERN_INFO "exFAT: checksum failed for super block\n");
	return -EINVAL;
}

static int exfat_fill_super(struct super_block *sb, void *data, int silent)
{
	struct exfat_super_block *exsb;
	struct exfat_sb_info *sbi;
	struct buffer_head *bh = NULL;
	struct inode *root_inode;
	unsigned long blocksize;
	int err;

	sbi = kzalloc(sizeof(struct exfat_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;

	sb->s_magic = EXFAT_SUPER_MAGIC;
	sb->s_maxbytes = MAX_LFS_FILESIZE; /* FIXME: ~0ULL? */
	sb->s_op = &exfat_sops;
	sb->s_time_gran = 10000000; /* 10ms: FIXME: but atime is 2sec */
/*	sb->s_export_op = &exfat_export_ops; FIXME: don't support nfs? */
	sbi->opts.nls = NULL;
	sbi->bitmap_inode = NULL;
	sbi->upcase = NULL;

	if (!sb_min_blocksize(sb, EXFAT_MIN_BLKSIZE)) {
		printk(KERN_ERR "exFAT: unable to set block size\n");
		err = -EINVAL;
		goto error;
	}

	bh = sb_bread(sb, 0);
	if (!bh) {
		printk(KERN_ERR "exFAT: unable to read sector 0\n");
		err = -EIO;
		goto error;
	}
	exsb = (struct exfat_super_block *)bh->b_data;

	sbi->fat_blocknr = le32_to_cpu(exsb->fat_blocknr);
	sbi->fat_block_counts = le32_to_cpu(exsb->fat_block_counts);
	sbi->clus_blocknr = le32_to_cpu(exsb->clus_blocknr);
	sbi->total_clusters = le32_to_cpu(exsb->total_clusters);
	sbi->rootdir_clusnr = le32_to_cpu(exsb->rootdir_clusnr);
	sbi->state = le16_to_cpu(exsb->state);
	sbi->bpc_bits = exsb->block_per_clus_bits;
	sbi->bpc = 1 << exsb->block_per_clus_bits;
	sbi->serial_number = le32_to_cpu(exsb->serial_number);

	err = exfat_exsb_validate(sb, exsb, silent);
	if (err)
		goto error;

	blocksize = 1 << exsb->blocksize_bits;
	brelse(bh);
	bh = NULL;

	if (sb->s_blocksize != blocksize) {
		err = -EINVAL;
#if 0
		/* FIXME: Windows supports this? */
		if (!sb_set_blocksize(sb, blocksize)) {
			printk(KERN_ERR "exFAT: unable to set block size\n");
			goto error;
		}
#else
		printk(KERN_ERR "exFAT: blocksize is not same with"
		       " sector size\n");
		goto error;
#endif
	}

	exfat_hash_init(sb);
	sbi->clus_size = sb->s_blocksize << sbi->bpc_bits;
	sbi->clus_bits = sb->s_blocksize_bits + sbi->bpc_bits;
	sbi->fpb = sb->s_blocksize >> EXFAT_ENT_BITS;
	sbi->fpb_bits = sb->s_blocksize_bits - EXFAT_ENT_BITS;

	err = exfat_exsb_verify_checksum(sb, silent);
	if (err)
		goto error;

	err = parse_options(&sbi->opts, data, silent, 0);
	if (err)
		goto error;

	root_inode = exfat_rootdir_iget(sb);
	if (IS_ERR(root_inode)) {
		err = PTR_ERR(root_inode);
		goto error;
	}

	err = exfat_read_rootdir(root_inode);
	if (err)
		goto error_iput;

	if (sbi->state & EXFAT_SB_DIRTY) {
		printk(KERN_WARNING "exFAT: filesystem is not clean\n");
		/* FIXME: we need to do something? */
	}

	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root) {
		printk(KERN_ERR "exFAT: couldn't get root dentry\n");
		goto error_iput;
	}
	sb->s_root->d_op = &exfat_dentry_ops;

	return 0;

error_iput:
	iput(root_inode);
error:
	brelse(bh);

	exfat_put_super(sb);

	return err;
}

static int exfat_get_sb(struct file_system_type *fs_type, int flags,
			const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_bdev(fs_type, flags, dev_name, data,
			   exfat_fill_super, mnt);
}

static struct file_system_type exfat_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "exfat",
	.get_sb		= exfat_get_sb,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init exfat_fs_init(void)
{
	int err;

	err = exfat_init_cache_cachep();
	if (err)
		goto error;
	err = exfat_init_inodecache();
	if (err)
		goto error_cache;
	err = register_filesystem(&exfat_fs_type);
	if (err)
		goto error_inode;
	return 0;

error_inode:
	exfat_destroy_inodecache();
error_cache:
	exfat_destroy_cache_cachep();
error:
	return err;
}

static void __exit exfat_fs_exit(void)
{
	unregister_filesystem(&exfat_fs_type);
	exfat_destroy_inodecache();
	exfat_destroy_cache_cachep();
}

module_init(exfat_fs_init)
module_exit(exfat_fs_exit)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("exFAT Filesystem");
MODULE_AUTHOR("OGAWA Hirofumi");
