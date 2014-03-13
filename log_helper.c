#include "log.h"
#include "protocol.h"
#include "protocol_net.h"
#include "hash_transaction.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <openssl/bn.h>

/* FIXME: Generate from headers! */
void log_add_struct_(struct log *log, const char *structname, const void *ptr)
{
	if (streq(structname, "struct protocol_double_sha")) {
		const struct protocol_double_sha *s = ptr;
		log_add(log,
			"%02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x",
			s->sha[0], s->sha[1], s->sha[2], s->sha[3],
			s->sha[4], s->sha[5], s->sha[6], s->sha[7],
			s->sha[8], s->sha[9], s->sha[10], s->sha[11],
			s->sha[12], s->sha[13], s->sha[14], s->sha[15],
			s->sha[16], s->sha[17], s->sha[18], s->sha[19],
			s->sha[20], s->sha[21], s->sha[22], s->sha[23],
			s->sha[24], s->sha[25], s->sha[26], s->sha[27],
			s->sha[28], s->sha[29], s->sha[30], s->sha[31]);
	} else if (streq(structname, "struct protocol_net_address")) {
		const struct protocol_net_address *addr = ptr;
		char str[INET6_ADDRSTRLEN];

		if (inet_ntop(AF_INET6, addr->addr, str, sizeof(str)) == NULL)
			log_add(log, "Unconvertable IPv6 (%s)",
				strerror(errno));
		else
			log_add(log, "%s", str);
		log_add(log, ":%u", be16_to_cpu(addr->port));
	} else if (streq(structname, "union protocol_transaction")) {
		const union protocol_transaction *t = ptr;
		struct protocol_double_sha sha;

		hash_transaction(t, NULL, 0, &sha);
		switch (t->hdr.type) {
		case TRANSACTION_NORMAL:
			log_add(log, "NORMAL %u inputs => %u (%u change) ",
				le32_to_cpu(t->normal.num_inputs),
				le32_to_cpu(t->normal.send_amount),
				le32_to_cpu(t->normal.change_amount));
			break;
		case TRANSACTION_FROM_GATEWAY:
			log_add(log, "GATEWAY %u outputs ",
				le32_to_cpu(t->gateway.num_outputs));
			break;
		default:
			log_add(log, "UNKNOWN(%u) ", t->hdr.type);
			break;
		}
		log_add_struct(log, struct protocol_double_sha, &sha);
	} else if (streq(structname, "BIGNUM")) {
		char *str = BN_bn2hex(ptr);
		log_add(log, "%s", str);
		OPENSSL_free(str);
	} else
		abort();
}

void log_add_enum_(struct log *log, const char *enumname, unsigned val)
{
	const char *name = NULL;
	if (streq(enumname, "enum protocol_req_type")
	    || streq(enumname, "enum protocol_resp_type")) {
		switch (val) {
		case PROTOCOL_REQ_WELCOME:
			name = "PROTOCOL_REQ_WELCOME"; break;
		case PROTOCOL_REQ_ERR:
			name = "PROTOCOL_REQ_ERR"; break;
		case PROTOCOL_REQ_NEW_BLOCK:
			name = "PROTOCOL_REQ_NEW_BLOCK"; break;
		case PROTOCOL_REQ_NEW_TRANSACTION:
			name = "PROTOCOL_REQ_NEW_TRANSACTION"; break;
		case PROTOCOL_REQ_BATCH:
			name = "PROTOCOL_REQ_BATCH"; break;
		case PROTOCOL_REQ_TRANSACTION_NUMS:
			name = "PROTOCOL_REQ_TRANSACTION_NUMS"; break;
		case PROTOCOL_REQ_TRANSACTION:
			name = "PROTOCOL_REQ_TRANSACTION"; break;
		case PROTOCOL_RESP_WELCOME:
			name = "PROTOCOL_RESP_WELCOME"; break;
		case PROTOCOL_RESP_ERR:
			name = "PROTOCOL_RESP_ERR"; break;
		case PROTOCOL_RESP_NEW_BLOCK:
			name = "PROTOCOL_RESP_NEW_BLOCK"; break;
		case PROTOCOL_RESP_NEW_TRANSACTION:
			name = "PROTOCOL_RESP_NEW_TRANSACTION"; break;
		case PROTOCOL_RESP_BATCH:
			name = "PROTOCOL_RESP_BATCH"; break;
		case PROTOCOL_RESP_TRANSACTION_NUMS:
			name = "PROTOCOL_RESP_TRANSACTION_NUMS"; break;
		case PROTOCOL_RESP_TRANSACTION:
			name = "PROTOCOL_RESP_TRANSACTION"; break;
		}
	} else if (streq(enumname, "enum protocol_error")) {
		switch (val) {
		case PROTOCOL_ERROR_NONE:
			name = "PROTOCOL_ERROR_NONE"; break;
		case PROTOCOL_UNKNOWN_COMMAND:
			name = "PROTOCOL_UNKNOWN_COMMAND"; break;
		case PROTOCOL_INVALID_LEN:
			name = "PROTOCOL_INVALID_LEN"; break;
		case PROTOCOL_SHOULD_BE_WAITING:
			name = "PROTOCOL_SHOULD_BE_WAITING"; break;
		case PROTOCOL_INVALID_RESPONSE:
			name = "PROTOCOL_INVALID_RESPONSE"; break;
		case PROTOCOL_ERROR_HIGH_VERSION:
			name = "PROTOCOL_ERROR_HIGH_VERSION"; break;
		case PROTOCOL_ERROR_LOW_VERSION:
			name = "PROTOCOL_ERROR_LOW_VERSION"; break;
		case PROTOCOL_ERROR_NO_INTEREST:
			name = "PROTOCOL_ERROR_NO_INTEREST"; break;
		case PROTOCOL_ERROR_WRONG_GENESIS:
			name = "PROTOCOL_ERROR_WRONG_GENESIS"; break;
		case PROTOCOL_ERROR_BLOCK_HIGH_VERSION:
			name = "PROTOCOL_ERROR_BLOCK_HIGH_VERSION"; break;
		case PROTOCOL_ERROR_BLOCK_LOW_VERSION:
			name = "PROTOCOL_ERROR_BLOCK_LOW_VERSION"; break;
		case PROTOCOL_ERROR_UNKNOWN_PREV:
			name = "PROTOCOL_ERROR_UNKNOWN_PREV"; break;
		case PROTOCOL_ERROR_BAD_TIMESTAMP:
			name = "PROTOCOL_ERROR_BAD_TIMESTAMP"; break;
		case PROTOCOL_ERROR_BAD_PREV_MERKLES:
			name = "PROTOCOL_ERROR_BAD_PREV_MERKLES"; break;
		case PROTOCOL_ERROR_BAD_DIFFICULTY:
			name = "PROTOCOL_ERROR_BAD_DIFFICULTY"; break;
		case PROTOCOL_ERROR_INSUFFICIENT_WORK:
			name = "PROTOCOL_ERROR_INSUFFICIENT_WORK"; break;
		case PROTOCOL_ERROR_TRANS_HIGH_VERSION:
			name = "PROTOCOL_ERROR_TRANS_HIGH_VERSION"; break;
		case PROTOCOL_ERROR_TRANS_LOW_VERSION:
			name = "PROTOCOL_ERROR_TRANS_LOW_VERSION"; break;
		case PROTOCOL_ERROR_TRANS_UNKNOWN:
			name = "PROTOCOL_ERROR_TRANS_UNKNOWN"; break;
		case PROTOCOL_ERROR_TRANS_BAD_GATEWAY:
			name = "PROTOCOL_ERROR_TRANS_BAD_GATEWAY"; break;
		case PROTOCOL_ERROR_TRANS_CROSS_SHARDS:
			name = "PROTOCOL_ERROR_TRANS_CROSS_SHARDS"; break;
		case PROTOCOL_ERROR_TOO_LARGE:
			name = "PROTOCOL_ERROR_TOO_LARGE"; break;
		case PROTOCOL_ERROR_TRANS_BADSIG:
			name = "PROTOCOL_ERROR_TRANS_BADSIG"; break;
		case PROTOCOL_ERROR_UNKNOWN_BLOCK:
			name = "PROTOCOL_ERROR_UNKNOWN_BLOCK"; break;
		case PROTOCOL_ERROR_BAD_BATCHNUM:
			name = "PROTOCOL_ERROR_BAD_BATCHNUM"; break;
		case PROTOCOL_ERROR_UNKNOWN_BATCH:
			name = "PROTOCOL_ERROR_UNKNOWN_BATCH"; break;
		case PROTOCOL_ERROR_DISAGREE_BATCHNUM:
			name = "PROTOCOL_ERROR_DISAGREE_BATCHNUM"; break;
		case PROTOCOL_ERROR_DISAGREE_BATCHSIZE:
			name = "PROTOCOL_ERROR_DISAGREE_BATCHSIZE"; break;
		}
	}
	if (name)
		log_add(log, "%s", name);
	else
		log_add(log, "Unknown %s (%u)", enumname, val);
}