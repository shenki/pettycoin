#include "../block.c"
#include "../create_tx.c"
#include "../marshal.c"
#include "../minimal_log.c"
#include "../hash_tx.c"
#include "../signature.c"
#include "../shadouble.c"
#include "../block_shard.c"
#include "../merkle_recurse.c"
#include "../proof.c"
#include "../difficulty.c"
#include "helper_key.h"
#include "helper_gateway_key.h"

/* AUTOGENERATED MOCKS START */
/* Generated stub for check_block */
void check_block(struct state *state, const struct block *block, bool all)
{ fprintf(stderr, "check_block called!\n"); abort(); }
/* Generated stub for check_chains */
void check_chains(struct state *state, bool all)
{ fprintf(stderr, "check_chains called!\n"); abort(); }
/* Generated stub for check_tx */
enum protocol_ecode check_tx(struct state *state, const union protocol_tx *tx,
			     const struct block *inside_block)
{ fprintf(stderr, "check_tx called!\n"); abort(); }
/* Generated stub for check_tx_inputs */
enum input_ecode check_tx_inputs(struct state *state,
				 const struct block *block,
				 const struct txhash_elem *me,
				 const union protocol_tx *tx,
				 unsigned int *bad_input_num)
{ fprintf(stderr, "check_tx_inputs called!\n"); abort(); }
/* Generated stub for merkle_some_txs */
void merkle_some_txs(const struct block_shard *shard,
		     size_t off, size_t max,
		     struct protocol_double_sha *merkle)
{ fprintf(stderr, "merkle_some_txs called!\n"); abort(); }
/* Generated stub for merkle_txs */
void merkle_txs(const struct block_shard *shard,
		struct protocol_double_sha *merkle)
{ fprintf(stderr, "merkle_txs called!\n"); abort(); }
/* Generated stub for pending_features */
u8 pending_features(const struct block *block)
{ fprintf(stderr, "pending_features called!\n"); abort(); }
/* Generated stub for save_block */
void save_block(struct state *state, struct block *new)
{ fprintf(stderr, "save_block called!\n"); abort(); }
/* Generated stub for update_block_ptrs_new_block */
void update_block_ptrs_new_block(struct state *state, struct block *block)
{ fprintf(stderr, "update_block_ptrs_new_block called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

static struct block *mock_block(const tal_t *ctx)
{
	struct block *b = tal(ctx, struct block);
	struct protocol_block_header *hdr;
	u8 *shard_nums;
	unsigned int i;

	/* minimal requirements to work for these tests. */
	b->hdr = hdr = tal(b, struct protocol_block_header);
	hdr->shard_order = 2;
	b->shard_nums = shard_nums = tal_arrz(b, u8, 1 << 2);
	shard_nums[1] = 2;
	b->shard = tal_arr(b, struct block_shard *, 1 << 2);

	for (i = 0; i < num_shards(hdr); i++)
		b->shard[i] = new_block_shard(b->shard, i, shard_nums[i]);

	return b;
}

int main(void)
{
	struct block_shard *shard;
	tal_t *ctx = tal(NULL, char);
	struct protocol_input_ref refs[4];
	struct protocol_gateway_payment payment[1];
	const union protocol_tx *tx1, *tx2;
	struct txptr_with_ref txp1, txp2;
	struct protocol_input input[1];
	struct protocol_txrefhash txrhash, scratch;
	const struct protocol_txrefhash *txrhp;
	struct block *b = mock_block(ctx);
	unsigned int i;

	/* These work with an empty block. */
	for (i = 0; i < num_shards(b->hdr); i++) {
		if (i == 1) {
			assert(!shard_all_known(b->shard[i]));
			assert(!shard_all_hashes(b->shard[i]));
		} else {
			/* Empty shards are always known. */
			assert(shard_all_known(b->shard[i]));
			assert(shard_all_hashes(b->shard[i]));
		}
	}

	shard = new_block_shard(ctx, 1, 2);
	assert(tal_parent(shard) == ctx);
	assert(shard->shardnum == 1);
	assert(tx_for(shard, 0) == NULL);

	/* Sew it into block as shard 1. */
	b->shard[1] = shard;
	assert(!shard_all_known(shard));
	assert(!shard_all_hashes(shard));

	/* Single payment. */
	payment[0].send_amount = cpu_to_le32(1000);
	payment[0].output_addr = *helper_addr(0);
	tx1 = create_from_gateway_tx(ctx, helper_gateway_public_key(),
				     1, payment, true, helper_gateway_key(ctx));
	assert(tal_parent(tx1) == ctx);
	assert(num_inputs(tx1) == 0);
	
	/* Test txptr_with_ref on tx without any refs. */
	txp1 = txptr_with_ref(ctx, tx1, NULL);
	assert(tal_parent(txp1.tx) == ctx);
	memcmp(txp1.tx, tx1, marshal_tx_len(tx1));
	memcmp(refs_for(txp1), (void *)1, marshal_input_ref_len(tx1));

	/* Now try with some refs. */
	hash_tx(tx1, &input[0].input);
	input[0].output = cpu_to_le16(0);
	input[0].unused = cpu_to_le16(0);

	tx2 = create_normal_tx(ctx, helper_addr(1),
			       500, 500, 1, false, input,
			       helper_private_key(ctx, 0));
	assert(tal_parent(tx2) == ctx);
	assert(num_inputs(tx2) == 1);

	refs[0].blocks_ago = cpu_to_le32(1);
	refs[0].shard = cpu_to_le16(0);
	refs[0].txoff = 0;
	refs[0].unused = 0;

	txp2 = txptr_with_ref(ctx, tx2, refs);
	assert(tal_parent(txp2.tx) == ctx);
	memcmp(txp2.tx, tx2, marshal_tx_len(tx2));
	memcmp(refs_for(txp2), refs, marshal_input_ref_len(tx2));

	/* Put the first one in the shard as a hash. */
	hash_tx(txp1.tx, &txrhash.txhash);
	hash_refs(refs_for(txp1), num_inputs(txp1.tx), &txrhash.refhash);

	bitmap_set_bit(shard->txp_or_hash, 0);
	shard->u[0].hash = &txrhash;
	shard->hashcount++;

	assert(!shard_all_known(shard));
	assert(!shard_all_hashes(shard));
	assert(!shard_is_tx(shard, 0));
	assert(tx_for(shard, 0) == NULL);

	/* Now put in second one as a tx. */
	shard->u[1].txp = txp2;
	shard->txcount++;
	assert(!shard_all_known(shard));
	assert(shard_all_hashes(shard));
	assert(shard_is_tx(shard, 1));
	assert(tx_for(shard, 1) == txp2.tx);
	assert(refs_for(shard->u[1].txp) == refs_for(txp2));

	/* Get txrefhash of hash */
	txrhp = txrefhash_in_shard(shard, 0, &scratch);
	assert(structeq(txrhp, &txrhash));

	/* Get txrefhash of tx (it will hash for us) */
	txrhp = txrefhash_in_shard(shard, 1, &scratch);
	hash_tx(tx2, &txrhash.txhash);
	hash_refs(refs, 1, &txrhash.refhash);
	assert(structeq(txrhp, &txrhash));

	/* Test block_get_tx */
	assert(block_get_tx(b, 1, 1) == txp2.tx);

	/* Test block_get_refs */
	assert(block_get_refs(b, 1, 1) == refs_for(txp2));

	tal_free(ctx);
	return 0;
}
