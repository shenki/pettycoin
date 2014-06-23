#include "genesis.h"
#include "protocol.h"

static struct protocol_block_header genesis_hdr = {
	.version = 1,
	.features_vote = 0,
	.nonce2 = { 0x53, 0x6f, 0x6d, 0x65, 0x20, 0x4e, 0x59, 0x54, 0x20, 0x48, 0x65, 0x61, 0x64, 0x20  },
	.fees_to = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  } }
};
static struct protocol_block_tailer genesis_tlr = {
	.timestamp = CPU_TO_LE32(1384494333),
	.difficulty = CPU_TO_LE32(0x1effffff),
	.nonce1 = CPU_TO_LE32(1271)
};
struct block genesis = {
	.hdr = &genesis_hdr,
	.tailer = &genesis_tlr,
	.sha = { { 0xff, 0x3e, 0x08, 0x86, 0x2e, 0xcb, 0x68, 0x4f, 0xed, 0x2d, 0x6d, 0x30, 0x12, 0xd8, 0x68, 0x49, 0x93, 0xf3, 0x63, 0x82, 0x67, 0xee, 0x7e, 0x48, 0x30, 0x19, 0xd2, 0xda, 0x7b, 0x93, 0x00, 0x00  }}
};