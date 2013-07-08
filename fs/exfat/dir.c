/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#include <linux/buffer_head.h>
#include "exfat.h"
#include "bitmap.h"
#include "upcase.h"

static void pd_init(struct exfat_parse_data *pd)
{
	pd->nr_bhs = 0;
#ifdef DEBUG
	pd->bh_cnt = 0;
#endif
}

static void pd_brelse(struct exfat_parse_data *pd)
{
	while (pd->nr_bhs) {
		pd->nr_bhs--;
		brelse(pd->bhs[pd->nr_bhs]);
#ifdef DEBUG
		pd->bh_cnt--;
#endif
	}
	pd_init(pd);
}

static void *pd_get_chunk(struct exfat_parse_data *pd)
{
	return pd->bhs[0]->b_data + pd->bh_offset;
}

struct exfat_parse_iter_data {
	unsigned long	left;
	unsigned long	bh_offset;
	unsigned int	bh_index;
};

static void *pd_iter_first_de(struct exfat_parse_data *pd,
			      struct exfat_parse_iter_data *pd_iter)
{
	DEBUG_ON(pd->size < EXFAT_CHUNK_SIZE, "pd->size %lu\n", pd->size);
	pd_iter->left = pd->size - EXFAT_CHUNK_SIZE;
	pd_iter->bh_offset = pd->bh_offset;
	pd_iter->bh_index = 0;
	return pd_get_chunk(pd);
}

static void *pd_iter_next_de(struct exfat_parse_data *pd,
			     struct exfat_parse_iter_data *pd_iter)
{
	if (pd_iter->left < EXFAT_CHUNK_SIZE)
		return NULL;
	pd_iter->left -= EXFAT_CHUNK_SIZE;
	pd_iter->bh_offset += EXFAT_CHUNK_SIZE;
	if (pd_iter->bh_offset >= pd->bhs[pd_iter->bh_index]->b_size) {
		pd_iter->bh_index++;
		pd_iter->bh_offset = 0;
	}
	return pd->bhs[pd_iter->bh_index]->b_data + pd_iter->bh_offset;
}

enum { PARSE_NEXT, PARSE_STOP, };

struct exfat_parse_callback {
	int		(*parse)(struct inode *, loff_t,
				 struct exfat_parse_data *,
				 struct exfat_parse_callback *);
	void		*data;
};

static int exfat_parse_dir(struct file *filp, struct inode *dir, loff_t *ppos,
			   struct exfat_parse_callback *pcb)
{
	struct super_block *sb = dir->i_sb;
	struct exfat_parse_data pd;
	struct buffer_head map_bh, *bh;
	sector_t uninitialized_var(blocknr), last_iblock;
	unsigned long blocksize, blocks, offset, de_left;
	loff_t pos;
	int ret, err;

	blocksize = 1 << dir->i_blkbits;
	last_iblock = dir->i_size >> dir->i_blkbits;

	/* FIXME: there is this limitation? Or limitation is cluster size? */
	if (dir->i_size & (blocksize - 1)) {
		exfat_fs_panic(sb, "invalid directory size (size %lld)",
			       dir->i_size);
		return -EIO;
	}

	pos = *ppos;

	/* FIXME: can cache the free space for next create()/mkdir()/etc? */
	pd_init(&pd);
	err = ret = 0;
	blocks = de_left = 0;
	while (!ret && pos < dir->i_size) {
		if (!blocks) {
			/* FIXME: readahead, probably last_iblock is too big */
			sector_t iblock;

			iblock = pos >> dir->i_blkbits;

			map_bh.b_state = 0;
			map_bh.b_blocknr = 0;
			map_bh.b_size = (last_iblock-iblock) << dir->i_blkbits;

			err = exfat_get_block(dir, iblock, &map_bh, 0);
			if (err)
				break;
			DEBUG0_ON(!buffer_mapped(&map_bh));

			blocknr = map_bh.b_blocknr;
			blocks = map_bh.b_size >> dir->i_blkbits;
		}

		offset = pos & (blocksize - 1);
		bh = sb_bread(sb, blocknr);
		if (!bh) {
			err = -EIO;
			break;
		}
#ifdef DEBUG
		pd.bh_cnt++;
#endif
		if (de_left) {
			unsigned long len;
#ifdef DEBUG
			pd.bh_cnt++;
#endif
			get_bh(bh);
			pd.bhs[pd.nr_bhs] = bh;
			pd.nr_bhs++;

			len = min(de_left, blocksize);
			de_left -= len;
			offset += len;
			pos += len;

			EXFAT_DEBUG("%s: blocknr %llu, de_left %lu, offset %lu,"
			       " pos %llu, len %lu\n", __func__,
			       (llu)bh->b_blocknr, de_left, offset, pos, len);
			if (!de_left)
				goto dirent_complete;

			goto next_block;
		}

		while (offset < bh->b_size) {
			struct exfat_chunk_dirent *dirent;
			unsigned long len, chunks;

			dirent = (void *)bh->b_data + offset;

			if (dirent->type == EXFAT_TYPE_EOD) {
				pos = dir->i_size;
				break;
			} else if (!(dirent->type & EXFAT_TYPE_VALID)) {
				offset += EXFAT_CHUNK_SIZE;
				pos += EXFAT_CHUNK_SIZE;
				continue;
			}

			if (dirent->type == EXFAT_TYPE_DIRENT)
				chunks = 1 + dirent->sub_chunks;
			else
				chunks = 1;

			de_left = chunks << EXFAT_CHUNK_BITS;

			pd.size = de_left;
#ifdef DEBUG
			pd.bh_cnt++;
#endif
			get_bh(bh);
			pd.bhs[0] = bh;
			pd.nr_bhs = 1;
			pd.bh_offset = offset;

			len = min(de_left, blocksize - offset);
			de_left -= len;
			offset += len;
			pos += len;

			EXFAT_DEBUG("%s: blocknr %llu, de_left %lu, offset %lu,"
			       " pos %llu, len %lu\n", __func__,
			       (llu)bh->b_blocknr, de_left, offset, pos, len);
			if (!de_left) {
dirent_complete:
				ret = pcb->parse(dir, pos - pd.size, &pd, pcb);
				pd_brelse(&pd);
				if (ret != PARSE_NEXT) {
					if (ret < 0)
						err = ret;
					pos -= pd.size;
					break;
				}
			}
		}
next_block:
		*ppos = pos;
		brelse(bh);
#ifdef DEBUG
		pd.bh_cnt--;
#endif
		blocknr++;
		blocks--;
	}
	pd_brelse(&pd);
#ifdef DEBUG
	DEBUG_ON(pd.bh_cnt, "pd.bh_cnt %d\n", pd.bh_cnt);
#endif

	return err;
}

/* FIXME: unicode string should be cached? */
static int exfat_d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	return 1;
}

static int exfat_d_hash(struct dentry *dentry, struct qstr *name)
{
	/* FIXME: can't cache converted name/hash? */
	return 0;
}

static int exfat_d_compare(struct dentry *dentry, struct qstr *a,
			   struct qstr *b)
{
	/* FIXME: can't use converted name/hash? */
	if (a->len != b->len)
		return 1;
	return memcmp(a->name, b->name, a->len);
}

static int exfat_d_delete(struct dentry *dentry)
{
	return 0;
}

static void exfat_d_release(struct dentry *dentry)
{
}

static void exfat_d_iput(struct dentry *dentry, struct inode *inode)
{
	iput(inode);
}

struct dentry_operations exfat_dentry_ops = {
	.d_revalidate	= exfat_d_revalidate,
	.d_hash		= exfat_d_hash,
	.d_compare	= exfat_d_compare,
	.d_delete	= exfat_d_delete,
	.d_release	= exfat_d_release,
	.d_iput		= exfat_d_iput,
};

static int exfat_match(struct exfat_sb_info *sbi, wchar_t *a, __le16 *b, u8 len)
{
	u8 i;
	for (i = 0; i < len; i++) {
		/* FIXME: assume a[i] is upper-case */
		if (a[i] != exfat_towupper(sbi, le16_to_cpu(b[i])))
			return 0;
	}
	return -1;
}

struct lookup_parse_data {
	struct inode *inode;
	wchar_t *wstr;
	u8 wlen;
	u16 hash;
};

static int lookup_parse(struct inode *dir, loff_t pos,
			struct exfat_parse_data *pd,
			struct exfat_parse_callback *pcb)
{
	struct super_block *sb = dir->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct lookup_parse_data *pcb_data = pcb->data;
	struct exfat_parse_iter_data pd_iter;
	struct exfat_chunk_dirent *dirent;
	struct exfat_chunk_data *data;
	struct exfat_chunk_name *name;
	wchar_t *wstr;
	u16 csum;
	u8 len, de_namelen;

	dirent = pd_iter_first_de(pd, &pd_iter);
	if (dirent->type != EXFAT_TYPE_DIRENT)
		return PARSE_NEXT;
	/* ->sub_chunk was already checked by caller. */
	DEBUG_ON((1 + dirent->sub_chunks) << EXFAT_CHUNK_BITS != pd->size,
		 "sub_chunks %u, pd->size %lu\n", dirent->sub_chunks, pd->size);
	EXFAT_DEBUG("exFAT: 0x%02x found\n", dirent->type);
	/* Skip dirent->checksum field */
	csum = exfat_checksum16(0, dirent, 2);
	csum = exfat_checksum16(csum, (u8 *)dirent + 4, EXFAT_CHUNK_SIZE - 4);

	data = pd_iter_next_de(pd, &pd_iter);
	if (!data)
		return PARSE_NEXT;
	if (data->type != EXFAT_TYPE_DATA)
		return PARSE_NEXT;
	EXFAT_DEBUG("%s: wlen %u, name_len %u\n", __func__,
	       pcb_data->wlen, data->name_len);
	if (pcb_data->wlen != data->name_len)
		return PARSE_NEXT;
	EXFAT_DEBUG("%s: hash 0x%04x, checksum 0x%04x\n", __func__,
	       pcb_data->hash, le16_to_cpu(data->hash));
	if (pcb_data->hash != le16_to_cpu(data->hash))
		return PARSE_NEXT;
	de_namelen = data->name_len;
	csum = exfat_checksum16(csum, data, EXFAT_CHUNK_SIZE);

	wstr = pcb_data->wstr;
	while (de_namelen) {
		name = pd_iter_next_de(pd, &pd_iter);
		if (!name)
			return PARSE_NEXT;
		if (name->type != EXFAT_TYPE_NAME)
			return PARSE_NEXT;

		len = min_t(u8, de_namelen, EXFAT_CHUNK_NAME_SIZE);
		if (!exfat_match(sbi, wstr, name->name, len))
			return PARSE_NEXT;
		wstr += len;
		de_namelen -= len;

		csum = exfat_checksum16(csum, name, EXFAT_CHUNK_SIZE);
	}

	/* Checksum of the remaining entries (may not be exfat_chunk_name) */
	while ((name = pd_iter_next_de(pd, &pd_iter)) != NULL) {
		if (!(name->type & EXFAT_TYPE_SUBCHUNK))
			return PARSE_NEXT;
		csum = exfat_checksum16(csum, name, EXFAT_CHUNK_SIZE);
	}

	if (le16_to_cpu(dirent->checksum) != csum) {
		exfat_fs_panic(sb, "checksum failed for directory entry"
			       " in lookup (0x%04x != 0x%04x)",
			       le16_to_cpu(dirent->checksum), csum);
		return -EIO;
	}

	EXFAT_DEBUG("%s: found\n", __func__);

	pcb_data->inode = exfat_iget(sb, pd, dirent, data);
	if (IS_ERR(pcb_data->inode))
		return PTR_ERR(pcb_data->inode);

	return PARSE_STOP;
}

static struct dentry *exfat_lookup(struct inode *dir, struct dentry *dentry,
				   struct nameidata *nd)
{
	struct exfat_sb_info *sbi = EXFAT_SB(dir->i_sb);
	struct nls_table *nls = sbi->opts.nls;
	struct exfat_parse_callback pcb;
	struct lookup_parse_data pcb_data;
	const unsigned char *name;
	wchar_t *wstr;
	unsigned int i, name_left, wlen, wleft;
	loff_t pos;
	int err;

	/* FIXME: can't use converted name/hash? */

	wlen = wleft = min(dentry->d_name.len, EXFAT_MAX_NAMELEN);
	pcb_data.wstr = wstr = kmalloc(wlen * sizeof(*wstr), GFP_KERNEL);
	if (!wstr) {
		err = -ENOMEM;
		goto error;
	}

	name = dentry->d_name.name;
	name_left = dentry->d_name.len;
	/* FIXME: maybe toupper and hash should be computed at same time */
//	err = exfat_conv_c2u_for_cmp(nls, &name, &name_left, &wstr, &wleft);
	err = exfat_conv_c2u(nls, &name, &name_left, &wstr, &wleft);
	/* FIXME: shouldn't return -ENAMETOOLONG? */
	if (err)
		goto error;
	pcb_data.wlen = wlen - wleft;

	pcb_data.hash = 0;
	for (i = 0; i < pcb_data.wlen; i++) {
		__le16 uc;
		pcb_data.wstr[i] = exfat_towupper(sbi, pcb_data.wstr[i]);
		uc = cpu_to_le16(pcb_data.wstr[i]);
		pcb_data.hash = exfat_checksum16(pcb_data.hash, &uc, sizeof(uc));
	}

	pcb_data.inode = NULL;
	pcb.parse = lookup_parse;
	pcb.data = &pcb_data;

	/* FIXME: pos can be previous lookuped position. */
	pos = 0;
	err = exfat_parse_dir(NULL, dir, &pos, &pcb);
	if (err)
		goto error;

	kfree(pcb_data.wstr);

	/* FIXME: can cache the position for next lookup()?
	 * Assume sequential locality (stat() for all files after readdir()) */

	dentry->d_op = &exfat_dentry_ops;
	dentry = d_splice_alias(pcb_data.inode, dentry);
	if (dentry)
		dentry->d_op = &exfat_dentry_ops;
	return dentry;

error:
	kfree(pcb_data.wstr);
	return ERR_PTR(err);
}

struct readdir_parse_data {
	void *dirbuf;
	filldir_t filldir;
	void *str;
};

#define EXFAT_MAX_NAME_BUFSIZE	(EXFAT_MAX_NAMELEN * NLS_MAX_CHARSET_SIZE)
#define DO_READDIR_CHECKSUM

static int readdir_parse(struct inode *dir, loff_t pos,
			 struct exfat_parse_data *pd,
			 struct exfat_parse_callback *pcb)
{
	struct super_block *sb = dir->i_sb;
	struct nls_table *nls = EXFAT_SB(sb)->opts.nls;
	struct readdir_parse_data *pcb_data = pcb->data;
	struct exfat_parse_iter_data pd_iter;
	struct exfat_chunk_dirent *dirent;
	struct exfat_chunk_data *data;
	struct exfat_chunk_name *name;
	struct inode *inode;
	unsigned long ino;
	unsigned int d_type;
#ifdef DO_READDIR_CHECKSUM
	u16 csum;
#endif
	u8 de_namelen;
	const __le16 *ustr;
	unsigned char *str;
	unsigned int uleft, str_left, str_len;
	int err;

	/* FIXME: should cache whether checksum was done */

	dirent = pd_iter_first_de(pd, &pd_iter);
	if (dirent->type != EXFAT_TYPE_DIRENT)
		return PARSE_NEXT;
	/* ->sub_chunk was already checked by caller. */
	DEBUG_ON((1 + dirent->sub_chunks) << EXFAT_CHUNK_BITS != pd->size,
		 "sub_chunks %u, pd->size %lu\n", dirent->sub_chunks, pd->size);
	EXFAT_DEBUG("exFAT: 0x%02x found\n", dirent->type);
#ifdef DO_READDIR_CHECKSUM
	/* Skip dirent->checksum field */
	csum = exfat_checksum16(0, dirent, 2);
	csum = exfat_checksum16(csum, (u8 *)dirent + 4, EXFAT_CHUNK_SIZE - 4);
#endif

	data = pd_iter_next_de(pd, &pd_iter);
	if (!data)
		return PARSE_NEXT;
	if (data->type != EXFAT_TYPE_DATA)
		return PARSE_NEXT;
	de_namelen = data->name_len;
#ifdef DO_READDIR_CHECKSUM
	csum = exfat_checksum16(csum, data, EXFAT_CHUNK_SIZE);
#endif

	if (!pcb_data->str) {
		/* FIXME: too big, should be own kmem_cachep */
		pcb_data->str = kmalloc(EXFAT_MAX_NAME_BUFSIZE, GFP_KERNEL);
		if (!pcb_data->str)
			return -ENOMEM;
	}

	str = pcb_data->str;
	str_left = EXFAT_MAX_NAME_BUFSIZE;
	while (de_namelen) {
		name = pd_iter_next_de(pd, &pd_iter);
		if (!name)
			return PARSE_NEXT;
		if (name->type != EXFAT_TYPE_NAME)
			return PARSE_NEXT;

		ustr = name->name;
		uleft = min_t(u8, de_namelen, EXFAT_CHUNK_NAME_SIZE);
		de_namelen -= uleft;

		err = exfat_conv_u2c(nls, &ustr, &uleft, &str, &str_left);
		if (err) {
			/* FIXME: if char was invalid, what to do */
			if (err == -EINVAL) {
				printk(KERN_ERR "exFAT: invalid char 0x%04x in"
				       " file name\n", le16_to_cpu(*ustr));
			}
			/* FIXME: shouldn't return -ENAMETOOLONG */
			return err;
		}
#ifdef DO_READDIR_CHECKSUM
		csum = exfat_checksum16(csum, name, EXFAT_CHUNK_SIZE);
#endif
	}

	/* Checksum of the remaining entries (may not be exfat_chunk_name) */
	while ((name = pd_iter_next_de(pd, &pd_iter)) != NULL) {
		if (!(name->type & EXFAT_TYPE_SUBCHUNK))
			return PARSE_NEXT;
#ifdef DO_READDIR_CHECKSUM
		csum = exfat_checksum16(csum, name, EXFAT_CHUNK_SIZE);
#endif
	}
#ifdef DO_READDIR_CHECKSUM
	if (le16_to_cpu(dirent->checksum) != csum) {
		exfat_fs_panic(sb, "checksum failed for directory entry"
			       " in readdir (0x%04x != 0x%04x)",
			       le16_to_cpu(dirent->checksum), csum);
		return -EIO;
	}
#endif

	inode = exfat_ilookup(sb, pd->bhs[0]->b_blocknr, pd->bh_offset);
	if (inode) {
		ino = inode->i_ino;
		iput(inode);
	} else
		ino = iunique(sb, EXFAT_RESERVED_INO);

	if (dirent->attrib & cpu_to_le16(EXFAT_ATTR_DIR))
		d_type = DT_DIR;
	else
		d_type = DT_REG;

	str_len = EXFAT_MAX_NAME_BUFSIZE - str_left;

	str[0] = '\0';
	EXFAT_DEBUG("%s: ent %p, str %s, len %u, pos %lld, ino %lu, type 0x%x\n",
	       __func__, pcb_data->dirbuf, (char *)pcb_data->str, str_len,
	       pos, ino, d_type);

	if (pcb_data->filldir(pcb_data->dirbuf, pcb_data->str, str_len, pos,
			      ino, d_type))
		return PARSE_STOP;

	return PARSE_NEXT;
}

static int exfat_readdir(struct file *filp, void *dirbuf, filldir_t filldir)
{
	struct inode *dir = filp->f_path.dentry->d_inode;
	struct exfat_parse_callback pcb;
	struct readdir_parse_data pcb_data;
	loff_t pos;
	int err;

	/* exFAT doesn't have "." and "..", so returns fake entries */
	while (filp->f_pos < 2) {
		unsigned long ino;
		int len = filp->f_pos + 1;

		if (filp->f_pos == 0)
			ino = dir->i_ino;
		else
			ino = parent_ino(filp->f_path.dentry);

		EXFAT_DEBUG("%s: ent %p, .., len %u, pos %lld, ino %lu, type 0x%x\n",
		       __func__, dirbuf, len, filp->f_pos, ino, DT_DIR);
		if (filldir(dirbuf, "..", len, filp->f_pos, ino, DT_DIR))
			return 0;

		filp->f_pos++;
	}
	if (filp->f_pos > 2 && (filp->f_pos & (EXFAT_CHUNK_SIZE - 1)))
		return -ENOENT;

	pcb_data.dirbuf = dirbuf;
	pcb_data.filldir = filldir;
	pcb_data.str = NULL;
	pcb.parse = readdir_parse;
	pcb.data = &pcb_data;

	pos = filp->f_pos & ~(loff_t)(EXFAT_CHUNK_SIZE - 1);
	err = exfat_parse_dir(filp, dir, &pos, &pcb);
	if (pos < 2)
		filp->f_pos = 2;
	else
		filp->f_pos = pos;

	kfree(pcb_data.str);

	return err;
}

struct rootdir_parse_data {
	u32 bitmap_clusnr;
	u64 bitmap_size;
	u32 upcase_checksum;
	u32 upcase_clusnr;
	u64 upcase_size;
};

static int rootdir_parse(struct inode *dir, loff_t pos,
			 struct exfat_parse_data *pd,
			 struct exfat_parse_callback *pcb)
{
	struct exfat_chunk_bitmap *bitmap;
	struct exfat_chunk_upcase *upcase;
	struct rootdir_parse_data *pcb_data = pcb->data;

	bitmap = pd_get_chunk(pd);
	switch (bitmap->type) {
	case EXFAT_TYPE_BITMAP:
		if (pcb_data->bitmap_clusnr) {
			/* FIXME: for this, what do we do */
			printk(KERN_WARNING "exFAT: found another"
			       " free space bitmap, ignored\n");
			break;
		}
		pcb_data->bitmap_clusnr = le32_to_cpu(bitmap->clusnr);
		pcb_data->bitmap_size = le64_to_cpu(bitmap->size);
		break;
	case EXFAT_TYPE_UPCASE:
		if (pcb_data->upcase_clusnr) {
			/* FIXME: for this, what do we do */
			printk(KERN_WARNING "exFAT: found another"
			       " upper-case table, ignored\n");
			break;
		}
		upcase = pd_get_chunk(pd);
		pcb_data->upcase_checksum = le32_to_cpu(upcase->checksum);
		pcb_data->upcase_clusnr = le32_to_cpu(upcase->clusnr);
		pcb_data->upcase_size = le64_to_cpu(upcase->size);
		break;
	}

	return PARSE_NEXT;
}

int exfat_read_rootdir(struct inode *dir)
{
	struct super_block *sb = dir->i_sb;
	struct exfat_parse_callback pcb;
	struct rootdir_parse_data pcb_data;
	loff_t pos;
	int err;

	pcb_data.bitmap_clusnr = 0;
	pcb_data.upcase_clusnr = 0;
	pcb.parse = rootdir_parse;
	pcb.data = &pcb_data;

	pos = 0;
	err = exfat_parse_dir(NULL, dir, &pos, &pcb);
	if (err)
		return err;

	err = exfat_setup_bitmap(sb, pcb_data.bitmap_clusnr,
				 pcb_data.bitmap_size);
	if (err)
		return err;

	err = exfat_setup_upcase(sb, pcb_data.upcase_checksum,
				 pcb_data.upcase_clusnr,
				 pcb_data.upcase_size);
	if (err) {
		exfat_free_bitmap(EXFAT_SB(sb));
		return err;
	}

	return 0;
}

const struct inode_operations exfat_dir_inode_ops = {
//	.create		= ext3_create,
	.lookup		= exfat_lookup,
//	.link		= ext3_link,
//	.unlink		= ext3_unlink,
//	.symlink	= ext3_symlink,
//	.mkdir		= ext3_mkdir,
//	.rmdir		= ext3_rmdir,
//	.mknod		= ext3_mknod,
//	.rename		= ext3_rename,
//	.setattr	= ext3_setattr,
	.getattr	= exfat_getattr
#ifdef CONFIG_EXT3_FS_XATTR
//	.setxattr	= generic_setxattr,
//	.getxattr	= generic_getxattr,
//	.listxattr	= ext3_listxattr,
//	.removexattr	= generic_removexattr,
#endif
//	.permission	= ext3_permission,
	/* FIXME: why doesn't ext4 support this for directory? */
//	.fallocate	= ext4_fallocate,
//	.fiemap		= ext4_fiemap,
};

const struct file_operations exfat_dir_ops = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= exfat_readdir,
//	.unlocked_ioctl	= exfat_ioctl,
#ifdef CONFIG_COMPAT
//	.compat_ioctl	= exfat_compat_ioctl,
#endif
//	.fsync		= exfat_sync_file,
//	.release	= exfat_release_dir,
};
