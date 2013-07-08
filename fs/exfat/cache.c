/*
 * Copyright (C) 2008, OGAWA Hirofumi
 * Released under GPL v2.
 */

#include "exfat.h"
#include "cache.h"

/* This must be > 0. */
#define EXFAT_MAX_CACHE		8

#define cache_entry(x)		list_entry(x, struct exfat_cache, cache_list)

struct exfat_cache {
	struct list_head cache_list;
	u32	iclusnr;		/* cluster number on data of inode. */
	u32	clusnr;			/* cluster number on disk. */
	u32	len;			/* number of contiguous clusters */
};

static inline int exfat_max_cache(struct inode *inode)
{
	return EXFAT_MAX_CACHE;
}

static struct kmem_cache *exfat_cache_cachep;

static void exfat_cache_init_once(void *obj)
{
	struct exfat_cache *cache = (struct exfat_cache *)obj;
	INIT_LIST_HEAD(&cache->cache_list);
}

int __init exfat_init_cache_cachep(void)
{
	exfat_cache_cachep = kmem_cache_create("exfat_cache",
					sizeof(struct exfat_cache),
					0, SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD,
					exfat_cache_init_once);
	if (exfat_cache_cachep == NULL)
		return -ENOMEM;
	return 0;
}

void exfat_destroy_cache_cachep(void)
{
	kmem_cache_destroy(exfat_cache_cachep);
}

static inline struct exfat_cache *exfat_cache_alloc(struct inode *inode)
{
	return kmem_cache_alloc(exfat_cache_cachep, GFP_NOFS);
}

static inline void exfat_cache_free(struct exfat_cache *cache)
{
	DEBUG0_ON(!list_empty(&cache->cache_list));
	kmem_cache_free(exfat_cache_cachep, cache);
}

static inline void exfat_cache_update_lru(struct inode *inode,
					  struct exfat_cache *cache)
{
	struct exfat_inode_info *exi = EXFAT_I(inode);
	if (exi->cache_lru.next != &cache->cache_list)
		list_move(&cache->cache_list, &exi->cache_lru);
}

int exfat_cache_lookup(struct inode *inode, u32 iclusnr,
		       struct exfat_cache_id *cid)
{
	struct exfat_inode_info *exi = EXFAT_I(inode);
	struct exfat_cache *hit = NULL, *p;

	spin_lock(&exi->cache_lru_lock);
	list_for_each_entry(p, &exi->cache_lru, cache_list) {
		/* Find the cache of "iclusnr" or nearest cache. */
		if (p->iclusnr <= iclusnr) {
			if (!hit || hit->iclusnr < p->iclusnr) {
				hit = p;
				if (iclusnr < hit->iclusnr + hit->len)
					break;
			}
		}
	}
	if (hit) {
		exfat_cache_update_lru(inode, hit);

		cid->id = exi->cache_valid_id;
		cid->iclusnr = hit->iclusnr;
		cid->clusnr = hit->clusnr;
		cid->len = hit->len;
	}
	spin_unlock(&exi->cache_lru_lock);

	return hit != NULL;
}

static struct exfat_cache *exfat_cache_merge(struct inode *inode,
					     struct exfat_cache_id *new)
{
	struct exfat_cache *p;

	list_for_each_entry(p, &EXFAT_I(inode)->cache_lru, cache_list) {
		/* Find the same part as "new" in cluster-chain. */
		if (p->iclusnr == new->iclusnr) {
			DEBUG_ON(p->clusnr != new->clusnr,
				 "p->clusnr 0x%08x, new->clusnr 0x%08x\n",
				 p->clusnr, new->clusnr);
			if (new->len > p->len)
				p->len = new->len;
			return p;
		}
	}
	return NULL;
}

void exfat_cache_add(struct inode *inode, struct exfat_cache_id *new)
{
	struct exfat_inode_info *exi = EXFAT_I(inode);
	struct exfat_cache *cache, *tmp;

	/* unuseful cache */
	if (new->iclusnr == 0 && new->len < 2)
		return;

	spin_lock(&exi->cache_lru_lock);
	if (new->id != EXFAT_CACHE_VALID &&
	    new->id != exi->cache_valid_id)
		goto out;	/* This cache was invalidated */

	cache = exfat_cache_merge(inode, new);
	if (cache == NULL) {
		if (exi->nr_caches < exfat_max_cache(inode)) {
			exi->nr_caches++;
			spin_unlock(&exi->cache_lru_lock);

			tmp = exfat_cache_alloc(inode);
			spin_lock(&exi->cache_lru_lock);
			cache = exfat_cache_merge(inode, new);
			if (cache != NULL) {
				exi->nr_caches--;
				exfat_cache_free(tmp);
				goto out_update_lru;
			}
			cache = tmp;
		} else
			cache = cache_entry(exi->cache_lru.prev);

		cache->iclusnr = new->iclusnr;
		cache->clusnr = new->clusnr;
		cache->len = new->len;
	}
out_update_lru:
	exfat_cache_update_lru(inode, cache);
out:
	spin_unlock(&exi->cache_lru_lock);
}

/*
 * FIXME: cache should be more flexible. cache_invalid() should be
 * cache_truncate().
 */

/*
 * Cache invalidation occurs rarely, thus the LRU chain is not updated. It
 * fixes itself after a while.
 */
static void __exfat_cache_inval(struct inode *inode)
{
	struct exfat_inode_info *exi = EXFAT_I(inode);
	struct exfat_cache *cache;

	while (!list_empty(&exi->cache_lru)) {
		cache = cache_entry(exi->cache_lru.next);

		list_del_init(&cache->cache_list);
		exi->nr_caches--;
		exfat_cache_free(cache);
	}
	/* Update. The copy of caches before this id is discarded. */
	exi->cache_valid_id++;
	if (exi->cache_valid_id == EXFAT_CACHE_VALID)
		exi->cache_valid_id++;
}

void exfat_cache_inval(struct inode *inode)
{
	struct exfat_inode_info *exi = EXFAT_I(inode);

	spin_lock(&exi->cache_lru_lock);
	__exfat_cache_inval(inode);
	spin_unlock(&exi->cache_lru_lock);
}

void exfat_cache_inode_init_once(struct exfat_inode_info *exi)
{
	spin_lock_init(&exi->cache_lru_lock);
	INIT_LIST_HEAD(&exi->cache_lru);
	exi->nr_caches = 0;
	exi->cache_valid_id = EXFAT_CACHE_VALID + 1;
}
