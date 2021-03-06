#include "block.h"
#include "chain.h"
#include "check_block.h"
#include "complain.h"
#include "generating.h"
#include "peer.h"
#include "pending.h"
#include "shard.h"
#include "tal_arr.h"
#include "todo.h"
#include <ccan/cast/cast.h>
#include <ccan/structeq/structeq.h>
#include <time.h>

/* Simply block array helpers */
static void set_single(const struct block ***arr, const struct block *b)
{
	tal_resize(arr, 1);
	(*arr)[0] = b;
}

struct block *step_towards(const struct block *curr, const struct block *target)
{
	const struct block *prev_target;

	/* Move back towards target. */
	while (le32_to_cpu(curr->hdr->depth) > le32_to_cpu(target->hdr->depth))
		curr = curr->prev;

	/* Already past it, or equal to it */
	if (curr == target)
		return NULL;

	/* Move target back towards curr. */
	while (le32_to_cpu(target->hdr->depth) > le32_to_cpu(curr->hdr->depth)) {
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

/* Is a more work than b? */
static int cmp_work(const struct block *a, const struct block *b)
{
	return BN_cmp(&a->total_work, &b->total_work);
}

/* Find a link between a known and a longest chain. */
static bool find_connected_pair(const struct state *state,
				size_t *chain_num, size_t *known_num)
{
	unsigned int i, j;

	for (i = 0; i < tal_count(state->longest_chains); i++) {
		for (j = 0; j < tal_count(state->longest_knowns); j++) {
			if (block_preceeds(state->longest_knowns[j],
					   state->longest_chains[i])) {
				*chain_num = i;
				*known_num = j;
				return true;
			}
		}
	}
	return false;
}

void check_chains(struct state *state, bool all)
{
	const struct block *i;
	size_t n, num_next_level = 1;

	/* If multiple longest chains, all should have same work! */
	for (n = 1; n < tal_count(state->longest_chains); n++)
		assert(cmp_work(state->longest_chains[n],
				state->longest_chains[0]) == 0);

	/* If multiple longest known, all should have same work! */
	for (n = 1; n < tal_count(state->longest_knowns); n++)
		assert(cmp_work(state->longest_knowns[n],
				state->longest_knowns[0]) == 0);


	/* preferred_chain should be a descendent of longest_knowns[0] */
	for (i = state->preferred_chain;
	     i != state->longest_knowns[0];
	     i = i->prev) {
		assert(i != genesis_block(state));
		assert(!i->all_known);
	}

	/*
	 * preferred_chain is *not* state->longest_chains[0], then no
	 * chain should connect any longest_knowns to longest_chains.
	 */
	if (state->preferred_chain != state->longest_chains[0]) {
		size_t a, b;
		assert(!find_connected_pair(state, &a, &b));
	}

	/* We ignore blocks which have a problem. */
	assert(!state->preferred_chain->complaint);

	for (n = 0; n < tal_count(state->longest_knowns); n++)
		assert(!state->longest_knowns[n]->complaint);

	for (n = 0; n < tal_count(state->longest_chains); n++)
		assert(!state->longest_chains[n]->complaint);

	/* Checking the actual blocks is expensive! */
	if (!all)
		return;

	for (n = 0; n < tal_count(state->block_depth); n++) {
		size_t num_this_level = num_next_level;
		list_check(state->block_depth[n], "bad block depth");
		num_next_level = 0;
		list_for_each(state->block_depth[n], i, list) {
			const struct block *b;
			assert(le32_to_cpu(i->hdr->depth) == n);
			assert(num_this_level);
			num_this_level--;
			if (n == 0)
				assert(i == genesis_block(state));
			else {
				assert(structeq(&i->hdr->prev_block,
						&i->prev->sha));
				if (i->prev->complaint)
					assert(i->complaint);
			}
			assert(i->complaint ||
			       cmp_work(i, state->longest_chains[0]) <= 0);
			if (!i->complaint && i->all_known)
				assert(cmp_work(i, state->longest_knowns[0]) <= 0);
			
			list_for_each(&i->children, b, sibling) {
				num_next_level++;
				assert(b->prev == i);
			}
			check_block(state, i, all);
		}
			assert(num_this_level == 0);
	}
	assert(num_next_level == 0);
}

static void swap_blockptr(const struct block **a, const struct block **b)
{
	const struct block *tmp = *a;
	*a = *b;
	*b = tmp;
}

/* Search descendents to find if there's one with more work than bests. */
static void find_longest_descendents(const struct block *block,
				     const struct block ***bests)
{
	struct block *b;

	if (block->complaint)
		return;

	switch (cmp_work(block, (*bests)[0])) {
	case 1:
		/* Ignore previous bests, this is the best. */
		set_single(bests, block);
		break;
	case 0:
		/* Add to bests. */
		tal_arr_append(bests, block);
		break;
	}

	list_for_each(&block->children, b, sibling)
		find_longest_descendents(b, bests);
}

/* Returns true if it updated state->preferred_chain. */
static bool update_preferred_chain(struct state *state)
{
	const struct block **arr;

	/* Set up temporary array so we can use find_longest_descendents */
	arr = tal_arr(state, const struct block *, 1);
	arr[0] = state->longest_knowns[0];

	find_longest_descendents(arr[0], &arr);
	if (arr[0] == state->preferred_chain) {
		tal_free(arr);
		return false;
	}
	state->preferred_chain = arr[0];
	tal_free(arr);
	return true;
}

/*
 * If we have a choice of multiple "best" known blocks, we prefer the
 * one which leads to the longest known block.  Similarly, we prefer
 * longest chains if they're lead to by our longest known blocks.
 *
 * So, if we find such a pair, move them to the front.  Returns true
 * if they changed.
 */
static bool order_block_pointers(struct state *state)
{
	size_t chain, known;

	if (!find_connected_pair(state, &chain, &known))
		return false;

	if (chain == 0 && known == 0)
		return false;

	/* Swap these both to the front. */
	swap_blockptr(&state->longest_chains[0], &state->longest_chains[chain]);
	swap_blockptr(&state->longest_knowns[0], &state->longest_knowns[known]);
	return true;
}

/* Returns true if we changed state->longest_knowns. */
static bool update_known_recursive(struct state *state, struct block *block)
{
	struct block *b;
	bool knowns_changed;

	if (block->prev && !block->prev->all_known)
		return false;

	if (!block_all_known(block))
		return false;

	/* FIXME: Hack avoids writing to read-only genesis block. */
	if (!block->all_known)
		block->all_known = true;

	/* Blocks which are flawed are not useful */
	if (block->complaint)
		return false;

	switch (cmp_work(block, state->longest_knowns[0])) {
	case 1:
		log_debug(state->log, "New known block work ");
		log_add_struct(state->log, BIGNUM, &block->total_work);
		log_add(state->log, " exceeds old known work ");
		log_add_struct(state->log, BIGNUM,
			       &state->longest_knowns[0]->total_work);
		/* They're no longer longest, we are. */
		set_single(&state->longest_knowns, block);
		knowns_changed = true;
		break;
	case 0:
		tal_arr_append(&state->longest_knowns, block);
		knowns_changed = true;
		break;
	case -1:
		knowns_changed = false;
		break;
	}

	/* Check descendents. */
	list_for_each(&block->children, b, sibling) {
		if (update_known_recursive(state, b))
			knowns_changed = true;
	}
	return knowns_changed;
}

/* We're moving longest_known from old to new.  Dump all its transactions into
 * pending. */
static void steal_pending_txs(struct state *state,
			      const struct block *old,
			      const struct block *new)
{
	const struct block *end, *b;

	/* Traverse old path and take transactions */
	end = step_towards(new, old);
	if (end) {
		for (b = old; b != end->prev; b = b->prev)
			block_to_pending(state, b);
	}
}

/* We now know complete contents of block; update all_known for this
 * block (and maybe its descendents) and if necessary, update
 * longest_known and longest_known_descendent and restart generator.
 * Caller should wake_peers() if this returns true, in case they care.
 */
static bool update_known(struct state *state, struct block *block)
{
	const struct block *prev_known = state->longest_knowns[0];

	if (!update_known_recursive(state, block))
		return false;

	state->pending->needs_recheck = true;

	order_block_pointers(state);
	update_preferred_chain(state);

	if (state->longest_knowns[0] != prev_known) {
		/* Any transactions from old branch go into pending. */
		steal_pending_txs(state, prev_known, state->longest_knowns[0]);
	}

	/* FIXME: If we've timed out asking about preferred_chain or
	 * longest_knowns, refresh. */

	return true;
}

/* Now this block is the end of the longest chain.
 *
 * This matters because we want to know all about that chain so we can
 * mine it.  If everyone is sharing information normally, that should be
 * easy.
 */
static void new_longest(struct state *state, const struct block *block)
{
	if (block->pending_features && !state->upcoming_features) {
		/* Be conservative, halve estimate of time to confirm feature */
		time_t impact = le32_to_cpu(block->tailer->timestamp)
			+ (PROTOCOL_FEATURE_CONFIRM_DELAY
			   * PROTOCOL_BLOCK_TARGET_TIME(state->test_net) / 2);
		struct tm *when;

		when = localtime(&impact);

		/* FIXME: More prominent warning! */
		log_unusual(state->log,
			    "WARNING: unknown features 0x%02x voted in!",
			    block->pending_features);
		log_add(state->log, " Update your client! (Before %u-%u-%u)",
			when->tm_year, when->tm_mon, when->tm_mday);
		state->upcoming_features = block->pending_features;
	}

	/* We may prefer a different known (which leads to longest) now. */
	order_block_pointers(state);
	update_preferred_chain(state);
}

static void recheck_merkles(struct state *state, struct block *block)
{
	const struct block *bad_prev;
	u16 bad_prev_shard;

	if (!check_prev_txhashes(state, block, &bad_prev, &bad_prev_shard))
		complain_bad_prev_txhashes(state, block,
					   bad_prev, bad_prev_shard);
	else {
		struct block *b;

		/* FIXME: SLOW: We actually only need to check one byte of
		 * every 2^N-distance block */
		list_for_each(&block->children, b, sibling)
			recheck_merkles(state, b);
	}
}

static void update_block_ptrs_new_shard_or_empty(struct state *state,
						 struct block *block,
						 u16 shardnum)
{
	struct block *b;

	list_for_each(&block->children, b, sibling)
		recheck_merkles(state, block);
}

/* We've added a new block; update state->longest_chains, state->longest_knowns,
   state->longest_known_descendents as required. */
void update_block_ptrs_new_block(struct state *state, struct block *block)
{
	unsigned int i;

	switch (cmp_work(block, state->longest_chains[0])) {
	case 1:
		log_debug(state->log, "New block work ");
		log_add_struct(state->log, BIGNUM, &block->total_work);
		log_add(state->log, " exceeds old work ");
		log_add_struct(state->log, BIGNUM,
			       &state->longest_chains[0]->total_work);
		set_single(&state->longest_chains, block);
		new_longest(state, block);
		break;
	case 0:
		tal_arr_append(&state->longest_chains, block);
		new_longest(state, block);
		break;
	}

	/* Corner case for zero transactions: we can't call
	 * update_block_ptrs_new_shard() directly, since that would
	 * call update_known multiple times if block completely
	 * known, which breaks the longest_known[] calc.  */
	for (i = 0; i < num_shards(block->hdr); i++) {
		if (block->shard_nums[i] == 0)
			update_block_ptrs_new_shard_or_empty(state, block, i);
	}
	if (block_all_known(block)) {
		update_known(state, block);
	}

	/* FIXME: Only needed if a descendent of known[0] */
	update_preferred_chain(state);
}

/* Filled a new shard; update state->longest_chains, state->longest_knowns,
   state->longest_known_descendents as required. */
void update_block_ptrs_new_shard(struct state *state, struct block *block,
				 u16 shardnum)
{
	update_block_ptrs_new_shard_or_empty(state, block, shardnum);
	if (block_all_known(block)) {
		update_known(state, block);
	}
}

static void forget_about_all(struct state *state, const struct block *block)
{
	const struct block *b;

	todo_forget_about_block(state, &block->sha);
	list_for_each(&block->children, b, sibling)
		forget_about_all(state, b);
}

void update_block_ptrs_invalidated(struct state *state,
				   const struct block *block)
{
	const struct block *g = genesis_block(state);

	/* Brute force recalculation. */
	set_single(&state->longest_chains, g);
	set_single(&state->longest_knowns, g);
	state->preferred_chain = g;

	find_longest_descendents(g, &state->longest_chains);
	update_known(state, cast_const(struct block *, g));

	check_chains(state, false);

	/* We don't need to know anything about this or any decendents. */
	forget_about_all(state, block);

	/* Tell peers everything changed. */
	wake_peers(state);
}
