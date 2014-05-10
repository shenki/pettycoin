#include <ccan/tal/tal.h>
#include <ccan/err/err.h>
#include <openssl/bn.h>
#include <unistd.h>
#include "state.h"
#include "genesis.h"
#include "protocol_net.h"
#include "pseudorand.h"
#include "log.h"
#include "peer.h"
#include "pending.h"

struct state *new_state(bool test_net)
{
	struct state *s = tal(NULL, struct state);

	s->test_net = test_net;
	s->developer_test = false;
	s->block_depth = tal_arr(s, struct list_head *, 1);
	s->block_depth[0] = tal(s->block_depth, struct list_head);
	list_head_init(s->block_depth[0]);
	s->longest_chain = &genesis;
	s->longest_known = &genesis;
	s->longest_known_descendent = &genesis;
	thash_init(&s->thash);
	s->num_peers = 0;
	s->num_peers_connected = 0;
	list_head_init(&s->peers);
	s->upcoming_features = 0;
	s->refill_peers = true;
	s->peer_seeding = false;
	s->peer_cache = NULL;
	s->random_welcome = isaac64_next_uint64(isaac64);
	s->peer_seed_count = 0;
	s->log_level = LOG_BROKEN;
	s->log = new_log(s, NULL, "", s->log_level, STATE_LOG_MAX);
	s->generate = "pettycoin-generate";

	/* Set up genesis block */
	BN_init(&genesis.total_work);
	if (!BN_zero(&genesis.total_work))
		errx(1, "Failed to initialize genesis block");
	genesis.all_known = true;

	list_add_tail(s->block_depth[0], &genesis.list);
	s->pending = new_pending_block(s);
	return s;
}

void fatal(struct state *state, const char *fmt, ...)
{
	va_list ap;
	struct peer *peer;

	fprintf(stderr, "FATAL dumping logs:\n");

	va_start(ap, fmt);
	logv(state->log, LOG_BROKEN, fmt, ap);
	va_end(ap);

	/* Dump our log, then the peers. */
	log_to_file(STDERR_FILENO, state->log);
	list_for_each(&state->peers, peer, list)
		log_to_file(STDERR_FILENO, peer->log);

	exit(1);
}
	
