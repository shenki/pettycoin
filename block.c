#include <ccan/cast/cast.h>
#include "block.h"
#include "protocol.h"
#include "state.h"
#include "peer.h"
#include "generating.h"
#include "log.h"
#include "merkle_transactions.h"
#include "pending.h"
#include "packet.h"
#include "proof.h"
#include <string.h>

/* Is a more work than b? */
static bool more_work(const struct block *a, const struct block *b)
{
	return BN_cmp(&a->total_work, &b->total_work) > 0;
}

void check_chains(const struct state *state)
{
	const struct block *i;
	size_t n;

	for (n = 0; n < tal_count(state->block_depth); n++) {
		list_check(state->block_depth[n], "bad block depth");
		list_for_each(state->block_depth[n], i, list) {
			assert(i->blocknum == n);
			if (n == 0)
				assert(i == genesis_block(state));
			else {
				assert(memcmp(&i->hdr->prev_block, &i->prev->sha,
					      sizeof(i->prev->sha)) == 0);
				if (i->prev->complaint)
					assert(i->complaint);
			}
			assert(i->complaint ||
			       !more_work(i, state->longest_chain));
			if (!i->complaint && i->all_known)
				assert(!more_work(i, state->longest_known));
		}
	}

	/* longest_known_descendent should be a descendent of longest_known */
	for (i = state->longest_known_descendent;
	     i != state->longest_known;
	     i = i->prev) {
		assert(i != genesis_block(state));
		assert(!i->all_known);
	}

	/* We ignore blocks which have a problem. */
	assert(!state->longest_known->complaint);
	assert(!state->longest_known_descendent->complaint);
	assert(!state->longest_chain->complaint);
}

struct block *block_find(struct block *start, const u8 lower_sha[4])
{
	struct block *b = start;

	while (b) {
		if (memcmp(b->sha.sha, lower_sha, 4) == 0)
			break;

		b = b->prev;
	}
	return b;
}

/* Now this block is the end of the longest chain.
 *
 * This matters because we want to know all about that chain so we can
 * mine it.  If everyone is sharing information normally, that should be
 * easy.
 */
static void update_longest(struct state *state, const struct block *block)
{
	log_debug(state->log, "Longest moved from %u to %u ",
		  state->longest_chain->blocknum,
		  block->blocknum);
	log_add_struct(state->log, struct protocol_double_sha, &block->sha);

	state->longest_chain = block;

	/* We want peers to ask about contents of these blocks. */
	wake_peers(state);
}

/* Now this block is the end of the longest chain we know completely about.
 *
 * This is the best block to mine on.
 */
void update_longest_known(struct state *state, const struct block *block)
{
	const struct block *old = state->longest_known;

	log_debug(state->log, "Longest known moved from %u to %u ",
		  old->blocknum, block->blocknum);
	log_add_struct(state->log, struct protocol_double_sha, &block->sha);

	state->longest_known = block;

	/* Any transactions from old branch go into pending. */
	steal_pending_transactions(state, old, block);

	/* Restart generator on this block. */
	restart_generating(state);

	/* We want peers to tell others about contents of this block. */
	wake_peers(state);
}

/* Now this block is the end of the longest chain from longest_known.
 *
 * If we can't get information about the longest chain, we'd like
 * information about this chain.
 */
void update_longest_known_descendent(struct state *state,
				     const struct block *block)
{
	log_debug(state->log, "Longest known descendent moved from %u to %u ",
		  state->longest_known_descendent->blocknum,
		  block->blocknum);
	log_add_struct(state->log, struct protocol_double_sha, &block->sha);

	state->longest_known_descendent = block;

	/* We want peers to ask about contents of these blocks. */
	wake_peers(state);
}

void block_add(struct state *state, struct block *block)
{
	log_debug(state->log, "Adding block %u ", block->blocknum);
	log_add_struct(state->log, struct protocol_double_sha, &block->sha);

	/* Add to list for that generation. */
	if (block->blocknum >= tal_count(state->block_depth)) {
		/* We can only increment block depths. */
		assert(block->blocknum == tal_count(state->block_depth));
		tal_resize(&state->block_depth, block->blocknum + 1);
		state->block_depth[block->blocknum]
			= tal(state->block_depth, struct list_head);
		list_head_init(state->block_depth[block->blocknum]);
	}
	list_add_tail(state->block_depth[block->blocknum], &block->list);

	/* This can happen if precedessor has complaint. */
	if (block->complaint) {
		check_chains(state);
		/* It's not a candidate for real use. */
		return;
	}

	/* Is this the longest? */
	/* FIXME: if equal, do coinflip as per
	 * http://arxiv.org/pdf/1311.0243v2.pdf ?  Or GHOST? */
	if (more_work(block, state->longest_chain)) {
		log_debug(state->log, "New block work ");
		log_add_struct(state->log, BIGNUM, &block->total_work);
		log_add(state->log, " exceeds old work ");
		log_add_struct(state->log, BIGNUM,
			       &state->longest_chain->total_work);
		update_longest(state, block);
	}

	/* Corner case for zero transactions (will update
	 * longest_known_descendent if necessary). */
	if (le32_to_cpu(block->hdr->num_transactions) == 0)
		update_known(state, block);
	/* Have we just extended the longest known descendent? */
	else if (block->prev == state->longest_known_descendent)
		update_longest_known_descendent(state, block);
	check_chains(state);
}

/* FIXME: use hash table. */
struct block *block_find_any(struct state *state,
			     const struct protocol_double_sha *sha)
{
	int i, n = tal_count(state->block_depth);
	struct block *b;

	/* Search recent blocks first. */
	for (i = n - 1; i >= 0; i--) {
		list_for_each(state->block_depth[i], b, list) {
			if (memcmp(b->sha.sha, sha->sha, sizeof(sha->sha)) == 0)
				return b;
		}
	}
	return NULL;
}

u32 batch_max(const struct block *block, unsigned int batchnum)
{
	unsigned int num_trans, batch_start, full;

	/* How many could we possibly fit? */
	num_trans = le32_to_cpu(block->hdr->num_transactions);
	batch_start = batchnum << PETTYCOIN_BATCH_ORDER;

	full = num_trans - batch_start;
	if (full > (1 << PETTYCOIN_BATCH_ORDER))
		return (1 << PETTYCOIN_BATCH_ORDER);

	return full;
}

/* Do we have everything in this batch? */
bool batch_full(const struct block *block,
		const struct transaction_batch *batch)
{
	unsigned int batchnum = batch->trans_start >> PETTYCOIN_BATCH_ORDER;
	return batch->count == batch_max(block, batchnum);
}

bool block_full(const struct block *block, unsigned int *batchnum)
{
	unsigned int i, num;

	num = num_merkles(le32_to_cpu(block->hdr->num_transactions));
	for (i = 0; i < num; i++) {
		const struct transaction_batch *b = block->batch[i];
		if (batchnum)
			*batchnum = i;
		if (!b)
			return false;
		if (!batch_full(block, b))
			return false;
	}
	return true;
}

union protocol_transaction *block_get_trans(const struct block *block,
					    u32 trans_num)
{
	const struct transaction_batch *b;

	assert(trans_num < block->hdr->num_transactions);
	b = block->batch[batch_index(trans_num)];
	if (!b)
		return NULL;
	return cast_const(union protocol_transaction *,
			  b->t[trans_num % (1 << PETTYCOIN_BATCH_ORDER)]);
}

static void update_recursive(struct state *state,
			     struct block *block,
			     const struct block **best)
{
	if (!block->prev->all_known || !block_full(block, NULL))
		return;

	block->all_known = true;

	/* Blocks which are flawed are not useful */
	if (block->complaint)
		return;

	/* New winner to start mining on? */
	if (more_work(block, *best))
		*best = block;

	/* Check descendents. */
	if (block->blocknum + 1 < tal_count(state->block_depth)) {
		struct block *b;

		list_for_each(state->block_depth[block->blocknum + 1], b, list)
			if (b->prev == block)
				update_recursive(state, b, best);
	}
}

/* Search descendents to find if there's one with more work than *best. */
static void find_longest_descendent(struct state *state,
				    const struct block *block,
				    const struct block **best)
{
	struct block *b;

	if (block->blocknum + 1 >= tal_count(state->block_depth))
		return;

	list_for_each(state->block_depth[block->blocknum + 1], b, list) {
		if (b->prev != block)
			continue;

		if (b->complaint)
			continue;

		if (more_work(b, *best)) {
			*best = b;
			find_longest_descendent(state, b, best);
		}
	}
}

/* We now know complete contents of block; update all_known for this
 * block (and maybe its descendents) and if necessary, update
 * longest_known and longest_known_descendent and restart generator
 * and wake peers (who might care). */
void update_known(struct state *state, struct block *block)
{
	const struct block *longest_known = state->longest_known;

	update_recursive(state, block, &longest_known);
	if (longest_known != state->longest_known) {
		const struct block *longest_descendent;

		update_longest_known(state, longest_known);

		/* Can't check chains until we've updated longest_descendent! */
		longest_descendent = longest_known;
		find_longest_descendent(state, longest_known,
					&longest_descendent);
		if (longest_descendent != state->longest_known_descendent)
			update_longest_known_descendent(state,
							longest_descendent);
	}
}

bool block_preceeds(const struct block *a, const struct block *b)
{
	if (a == b)
		return true;

	if (a->blocknum >= b->blocknum)
		return false;

	return block_preceeds(a, b->prev);
}

struct block *step_towards(const struct block *curr, const struct block *target)
{
	const struct block *prev_target;

	/* Move back towards target. */
	while (curr->blocknum > target->blocknum)
		curr = curr->prev;

	/* Already past it, or equal to it */
	if (curr == target)
		return NULL;

	/* Move target back towards curr. */
	while (target->blocknum > curr->blocknum) {
		prev_target = target;
		target = target->prev;
	}

	/* Now move both back until they're at the common ancestor. */
	while (curr != target) {
		prev_target = target;
		target = target->prev;
		curr = curr->prev;
	}

	/* This is one step towards the target. */
	return cast_const(struct block *, prev_target);
}

/* Brute force calculation of longest_known and longest_known_descendent */
static void recalc_longest_known(struct state *state)
{
	state->longest_known_descendent
		= state->longest_known
		= genesis_block(state);

	update_known(state, cast_const(struct block *, genesis_block(state)));
}

/* Brute force calculation of longest_chain. */
static void recalc_longest_chain(struct state *state)
{
	state->longest_chain = genesis_block(state);
	find_longest_descendent(state, state->longest_chain,
				&state->longest_chain);
}

static void invalidate_block(struct state *state,
			     struct block *block,
			     const void *complaint)
{
	unsigned int n;

	/* Mark block. */
	block->complaint = complaint;	

	/* FIXME: Save complaint to blockfile! */

	/* Mark descendents. */
	for (n = block->blocknum; n < tal_count(state->block_depth); n++) {
		struct block *i;
		list_for_each(state->block_depth[n], i, list) {
			if (i->prev->complaint)
				i->complaint = i->prev->complaint;
		}
	}

	if (block_preceeds(block, state->longest_chain))
		recalc_longest_chain(state);

	/* These blocks no longer qualify for longest or longest known. */
	if (block_preceeds(block, state->longest_known_descendent))
		recalc_longest_known(state);

	check_chains(state);

	/* Tell everyone... */
	complain_to_peers(state, complaint);
}

static void
invalidate_block_bad_input(struct state *state,
			   struct block *block,
			   const union protocol_transaction *trans,
			   unsigned int bad_transnum,
			   unsigned int bad_input,
			   const union protocol_transaction *intrans)
{
	struct protocol_req_block_bad_trans_input *req;

	assert(le32_to_cpu(trans->hdr.type) == TRANSACTION_NORMAL);
	log_unusual(state->log, "Block %u ", block->blocknum);
	log_add_struct(state->log, struct protocol_double_sha, &block->sha);
	log_add(state->log, " invalid due to trans %u ", bad_transnum);
	log_add_struct(state->log, union protocol_transaction, trans);
	log_add(state->log, " with bad input %u ", bad_input);
	log_add_struct(state->log, union protocol_transaction, intrans);

	req = tal_packet(block, struct protocol_req_block_bad_trans_input,
			 PROTOCOL_REQ_BLOCK_BAD_TRANS_INPUT);
	req->block = block->sha;
	req->inputnum = cpu_to_le32(bad_input);
	create_proof(&req->proof, block, bad_transnum);

	tal_packet_append_trans(&req, trans);
	tal_packet_append_trans(&req, intrans);

	invalidate_block(state, block, req);
}

static void
invalidate_block_bad_amounts(struct state *state,
			     struct block *block,
			     const union protocol_transaction *trans,
			     unsigned int bad_transnum)
{
	struct protocol_req_block_bad_trans_amount *req;
	union protocol_transaction *input[TRANSACTION_MAX_INPUTS];
	unsigned int i;

	assert(le32_to_cpu(trans->hdr.type) == TRANSACTION_NORMAL);
	log_unusual(state->log, "Block %u ", block->blocknum);
	log_add_struct(state->log, struct protocol_double_sha, &block->sha);
	log_add(state->log, " invalid amounts in trans %u ", bad_transnum);
	log_add_struct(state->log, union protocol_transaction, trans);
	log_add(state->log, " with inputs: ");
	/* FIXME: What if input is pending? */
	for (i = 0; i < le32_to_cpu(trans->normal.num_inputs); i++) {
		input[i] = thash_gettrans(&state->thash,
					  &trans->normal.input[i].input);
		log_add_struct(state->log, union protocol_transaction, input[i]);
		log_add(state->log, " (output %u)",
			le16_to_cpu(trans->normal.input[i].output));
	}

	req = tal_packet(block, struct protocol_req_block_bad_trans_amount,
			 PROTOCOL_REQ_BLOCK_BAD_TRANS_AMOUNT);
	req->block = block->sha;
	create_proof(&req->proof, block, bad_transnum);

	tal_packet_append_trans(&req, trans);
	for (i = 0; i < le32_to_cpu(trans->normal.num_inputs); i++)
		tal_packet_append_trans(&req, input[i]);

	invalidate_block(state, block, req);
}

static void
invalidate_block_bad_transaction(struct state *state,
				 struct block *block,
				 enum protocol_error err,
				 const union protocol_transaction *trans,
				 unsigned int bad_transnum)
{
	struct protocol_req_block_trans_invalid *req;	

	log_unusual(state->log, "Block %u ", block->blocknum);
	log_add_struct(state->log, struct protocol_double_sha, &block->sha);
	log_add(state->log, " invalid due to trans %u ", bad_transnum);
	log_add_struct(state->log, union protocol_transaction, trans);
	log_add(state->log, " error ");
	log_add_enum(state->log, enum protocol_error, err);

	req = tal_packet(block, struct protocol_req_block_trans_invalid,
			 PROTOCOL_REQ_BLOCK_TRANS_INVALID);
	req->block = block->sha;
	req->error = cpu_to_le32(err);
	create_proof(&req->proof, block, bad_transnum);
	
	tal_packet_append_trans(&req, trans);

	invalidate_block(state, block, req);
}

void invalidate_block_misorder(struct state *state,
			       struct block *block,
			       unsigned int bad_transnum1,
			       unsigned int bad_transnum2)
{
	struct protocol_req_block_trans_misorder *req;	
	const union protocol_transaction *trans1, *trans2;

	trans1 = block_get_trans(block, bad_transnum1);
	trans2 = block_get_trans(block, bad_transnum2);

	log_unusual(state->log, "Block %u ", block->blocknum);
	log_add_struct(state->log, struct protocol_double_sha, &block->sha);
	log_add(state->log, " invalid due to misorder trans %u vs %u ",
		bad_transnum1, bad_transnum2);
	log_add_struct(state->log, union protocol_transaction, trans1);
	log_add(state->log, " vs ");
	log_add_struct(state->log, union protocol_transaction, trans2);

	req = tal_packet(block, struct protocol_req_block_trans_misorder,
			 PROTOCOL_REQ_BLOCK_TRANS_MISORDER);
	req->block = block->sha;
	create_proof(&req->proof1, block, bad_transnum1);
	create_proof(&req->proof2, block, bad_transnum2);

	tal_packet_append_trans(&req, trans1);
	tal_packet_append_trans(&req, trans2);

	invalidate_block(state, block, req);
}

/* See check_trans_normal_inputs: bad_input and bad_intrans are valid
 * iff err = PROTOCOL_ERROR_TRANS_BAD_INPUT. */
void invalidate_block_badtrans(struct state *state,
			       struct block *block,
			       enum protocol_error err,
			       unsigned int bad_transnum,
			       unsigned int bad_input,
			       union protocol_transaction *bad_intrans)
{
	union protocol_transaction *trans;

	trans = block_get_trans(block, bad_transnum);

	switch (err) {
	case PROTOCOL_ERROR_TRANS_HIGH_VERSION:
	case PROTOCOL_ERROR_TRANS_LOW_VERSION:
	case PROTOCOL_ERROR_TRANS_UNKNOWN:
	case PROTOCOL_ERROR_TOO_LARGE:
	case PROTOCOL_ERROR_TRANS_BAD_SIG:
		break;

	case PROTOCOL_ERROR_TRANS_BAD_GATEWAY:
	case PROTOCOL_ERROR_TRANS_CROSS_SHARDS:
		assert(trans->hdr.type == TRANSACTION_FROM_GATEWAY);
		break;

	case PROTOCOL_ERROR_TOO_MANY_INPUTS:
		assert(trans->hdr.type == TRANSACTION_NORMAL);
		break;

	case PROTOCOL_ERROR_TRANS_BAD_INPUT:
		assert(trans->hdr.type == TRANSACTION_NORMAL);
		/* FIXME: This means an unknown input.  We don't
		 * complain. */
		if (!bad_intrans)
			return;
		invalidate_block_bad_input(state, block,
					   trans, bad_transnum,
					   bad_input, bad_intrans);
		return;

	case PROTOCOL_ERROR_TRANS_BAD_AMOUNTS:
		assert(trans->hdr.type == TRANSACTION_NORMAL);
		invalidate_block_bad_amounts(state, block, trans, bad_transnum);
		return;

	default:
		log_broken(state->log,
			   "Unknown invalidate_block_badtrans error ");
		log_add_enum(state->log, enum protocol_error, err);
		abort();
	}

	/* Simple single-transaction error. */
	invalidate_block_bad_transaction(state, block, err, trans,
					 bad_transnum);
}
