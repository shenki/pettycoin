#ifndef PETTYCOIN_SYNC_H
#define PETTYCOIN_SYNC_H
#include "config.h"

struct peer;
struct block;
struct state;
struct protocol_pkt_sync;
struct protocol_pkt_horizon;
struct protocol_pkt_children;
struct protocol_pkt_get_children;
struct protocol_pkt_get_block;

/* Create sync packet for this peer (which will own it) */
void *sync_or_horizon_pkt(struct peer *peer, const struct block *mutual);

/* Return an error if there's something wrong, otherwise process. */
enum protocol_ecode recv_sync_pkt(struct peer *peer,
				  const struct protocol_pkt_sync *sync);

/* Return an error if there's something wrong, otherwise process. */
enum protocol_ecode recv_horizon_pkt(struct peer *peer,
				     const struct protocol_pkt_horizon *horiz);

/* Process protocol_pkt_get_children, fill in *reply if no error. */
enum protocol_ecode
recv_get_children(struct peer *peer,
		  const struct protocol_pkt_get_children *pkt,
		  void **reply);

/* Process protocol_pkt_children. */
enum protocol_ecode recv_children(struct peer *peer,
				  const struct protocol_pkt_children *pkt);

/* Process protocol_pkt_get_block */
enum protocol_ecode
recv_get_block(struct peer *peer,
	       const struct protocol_pkt_get_block *pkt,
	       void **reply);
#endif /* PETTYCOIN_SYNC_H */
