#include "block.h"
#include "txhash.h"

static struct txhash_elem *txhash_i(struct htable *ht,
				    const struct protocol_double_sha *sha,
				    struct txhash_elem *te,
				    struct txhash_iter *i,
				    size_t h)
{
	while (te) {
		if (txhash_eq(te, sha))
			break;
		te = htable_nextval(ht, &i->i, h);
	}
	return te;
}

struct txhash_elem *txhash_firstval(struct txhash *txhash,
				    const struct protocol_double_sha *sha,
				    struct txhash_iter *i)
{
	size_t h = txhash_hashfn(sha);

	return txhash_i(&txhash->raw, sha,
			htable_firstval(&txhash->raw, &i->i, h),
			i, h);
}

struct txhash_elem *txhash_nextval(struct txhash *txhash,
				   const struct protocol_double_sha *sha,
				   struct txhash_iter *i)
{
	size_t h = txhash_hashfn(sha);

	return txhash_i(&txhash->raw, sha,
			htable_nextval(&txhash->raw, &i->i, h),
			i, h);
}

/* Get the actual transaction, we don't care about which block it's in */
union protocol_tx *txhash_gettx(struct txhash *txhash,
				const struct protocol_double_sha *sha)
{
	struct txhash_iter i;
	struct txhash_elem *te;
	union protocol_tx *tx;

	/* It's possible that it be known in one, but not the others.
	 * This is due to the refs: if we don't know them, we don't
	 * mark tx as known. */
	for (te = txhash_firstval(txhash, sha, &i);
	     te;
	     te = txhash_nextval(txhash, sha, &i)) {
		if (!shard_is_tx(te->u.block->shard[te->shardnum], te->txoff))
			continue;

		tx = block_get_tx(te->u.block, te->shardnum, te->txoff);
		/* Can't be in hash if it doesn't exist. */
		assert(tx);
		return tx;
	}

	return NULL;
}

void txhash_del_tx(struct txhash *txhash,
		   struct block *block,
		   u16 shard,
		   u8 txoff,
		   const struct protocol_double_sha *sha)
{
	struct txhash_iter i;
	struct txhash_elem *te;

	for (te = txhash_firstval(txhash, sha, &i);
	     te;
	     te = txhash_nextval(txhash, sha, &i)) {
		if (te->u.block == block
		    && te->shardnum == shard
		    && te->txoff == txoff) {
			htable_delval(&txhash->raw, &i.i);
			tal_free(te);
			break;
		}
	}
}

void txhash_add_tx(struct txhash *txhash,
		   const tal_t *ctx,
		   struct block *block,
		   u16 shard,
		   u8 txoff,
		   const struct protocol_double_sha *sha)
{
	struct txhash_elem *te;

	/* Add a new one for this block. */
	te = tal(ctx, struct txhash_elem);
	te->u.block = block;
	te->shardnum = shard;
	te->txoff = txoff;
	te->sha = *sha;
	txhash_add(txhash, te);
}
