/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#include "exfat.h"
#include "cache.h"
#include "fatent.h"
#include "cluster.h"

static void calc_cmap(struct exfat_clus_map *cmap, struct exfat_cache_id *cid,
		      u32 iclusnr)
{
	u32 offset = iclusnr - cid->iclusnr;
	cmap->iclusnr = iclusnr;
	cmap->clusnr = cid->clusnr + offset;
	cmap->len = cid->len - offset;
}

/*
 * Caller must be guarantee it have no race with truncate().  I.e. the
 * range of "iclusnr" and "iclusnr + len" must not be truncated.
 */
int exfat_get_cluster(struct inode *inode, u32 iclusnr, u32 clus_len,
		      struct exfat_clus_map *cmap)
{
	struct super_block *sb = inode->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct exfat_inode_info *exi = EXFAT_I(inode);
	struct exfat_cache_id cid;
	struct exfat_ent fatent;
	u32 clusnr;
	int err = 0, found_target;

	DEBUG0_ON(!exi->clusnr);

	if (exi->data_flag == EXFAT_DATA_CONTIGUOUS) {
		DEBUG_ON(clus_len >
		  (i_size_read(inode) + (sbi->clus_size - 1)) >> sbi->clus_bits,
			 "clus_len %u, i_size %lld, clus_size %lu\n",
			 clus_len, i_size_read(inode), sbi->clus_size);
		/* FIXME: should check clus_len limit from i_size? */
		cmap->iclusnr = iclusnr;
		cmap->clusnr = exi->clusnr + iclusnr;
		cmap->len = clus_len;
		EXFAT_DEBUG("%s: contig: iclusnr %u, clusnr %u, len %u\n", __func__,
		       cmap->iclusnr, cmap->clusnr, cmap->len);
		return 0;
	}

	/*
	 * FIXME: use rw_semaphore (r: here, w: truncate), then we can
	 * do readahead of contiguous clusters and cache it. (This
	 * should optimize disk-seek very much if clusters is not
	 * fragmented heavily)
	 */

	/* Setup the start cluster to walk */
	if (!exfat_cache_lookup(inode, iclusnr, &cid))
		cache_init(&cid, 0, exi->clusnr);
	clusnr = cid.clusnr + cid.len - 1;

	/* Walk the cluster-chain */
	found_target = 0;
	exfat_ent_init(&fatent);
	EXFAT_DEBUG("%s: iclusnr %u, clus_len %u: cid.iclusnr %u, cid.clusnr %u,"
	       " cid.len %u, search start clusnr %u\n",
	       __func__, iclusnr, clus_len, cid.iclusnr, cid.clusnr, cid.len,
	       clusnr);
	while (cid.iclusnr + cid.len < iclusnr + clus_len) {
		/* Target iclusnr was found, so find contiguous clusters */
		if (!found_target && (iclusnr < cid.iclusnr + cid.len))
			found_target = 1;

		/* FIXME: should be readahead FAT blocks */
#if 0
		err = exfat_ent_read(inode, &fatent, clusnr, &clusnr);
#else
		{
			u32 prev = clusnr;
			err = exfat_ent_read(inode, &fatent, prev, &clusnr);
			EXFAT_DEBUG("%s: ent_read: clusnr %u, next clusnr %u\n",
			       __func__, prev, clusnr);
		}
#endif
		if (err < 0)
			goto out;

		if (exfat_ent_eof(sbi, clusnr)) {
			exfat_cache_add(inode, &cid);

			/* Return special cmap */
			cmap->iclusnr = cid.iclusnr + cid.len;
			cmap->clusnr = clusnr;
			cmap->len = 0;
			goto out;
		} else if (exfat_ent_bad(sbi, clusnr)) {
			printk(KERN_ERR "exFAT: found bad cluster entry"
			       " (start cluster 0x%08x)\n", exi->clusnr);
			err = -EIO;
			goto out;
		} else if (!exfat_ent_valid(sbi, clusnr)) {
			exfat_fs_panic(sb, "found invalid cluster chain"
				       " (start cluster 0x%08x, entry 0x%08x)",
				       exi->clusnr, clusnr);
			err = -EIO;
			goto out;
		}

		if (cache_contiguous(&cid, clusnr))
			cid.len++;
		else {
			if (found_target) {
				/* Found a discontiguous cluster */
				calc_cmap(cmap, &cid, iclusnr);
				exfat_cache_add(inode, &cid);

				cache_init(&cid, cid.iclusnr + cid.len, clusnr);
				exfat_cache_add(inode, &cid);
				goto out;
			}
			cache_init(&cid, cid.iclusnr + cid.len, clusnr);
		}

		/* Prevent the infinite loop of cluster chain */
		if (cid.iclusnr + cid.len > sbi->total_clusters) {
			exfat_fs_panic(sb, "detected the cluster chain loop"
				       " (start cluster 0x%08x)", exi->clusnr);
			err = -EIO;
			goto out;
		}
	}

	calc_cmap(cmap, &cid, iclusnr);
	exfat_cache_add(inode, &cid);
out:
	exfat_ent_release(&fatent);
	EXFAT_DEBUG("%s: result cmap: iclusnr %u, clusnr %u, len %u\n", __func__,
	       cmap->iclusnr, cmap->clusnr, cmap->len);

	return err;
}

int exfat_chain_add(struct inode *inode, int new_dclus, int nr_cluster)
{
	return 0;
}
