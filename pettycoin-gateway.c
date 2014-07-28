/* Very simplistic gateway:
 * - Doesn't do payment protocol.
 * - Has race when it's killed.
 * - Puts private key on cmdline.
 *
 * You'll need to have a running 'bitcoind -testnet -txindex -server'
 * (if you've run the bitcoind in testnet mode before without -txindex,
 *  you'll need to run bitcoind -testnet -txindex -reindex).
 */
#include "hex.h"
#include "json.h"
#include "pettycoin_dir.h"
#include "protocol.h"
#include <ccan/build_assert/build_assert.h>
#include <ccan/err/err.h>
#include <ccan/htable/htable_type.h>
#include <ccan/noerr/noerr.h>
#include <ccan/opt/opt.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/tal/grab_file/grab_file.h>
#include <ccan/tal/str/str.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Whee, we're on testnet, who cares? */
#define REQUIRED_CONFIRMATIONS 1

static const struct protocol_double_sha *
keyof(const struct protocol_double_sha *elem)
{
	return elem;
}

static size_t thash_sha(const struct protocol_double_sha *key)
{
	size_t ret;

	memcpy(&ret, key, sizeof(ret));
	return ret;
}

static bool thash_eq(const struct protocol_double_sha *a,
		     const struct protocol_double_sha *key)
{
	return memcmp(a, key, sizeof(*a)) == 0;
}

HTABLE_DEFINE_TYPE(struct protocol_double_sha, keyof, thash_sha, thash_eq,
		   thash);

/* Import gateway key:
   (eg generated by dumpprivkey mxxEy4ryr3R3f3EXXGKd2w8z4sWnsZMcb9)
	   importprivkey cSJqHe7RU9rZDDnHxgb39vvYxttNM83JsMkJ1EeYNPtuckGd4Qej rescan=false

   Put it in account "gateway":
	   setaccount mxxEy4ryr3R3f3EXXGKd2w8z4sWnsZMcb9 gateway

   Get transactions:
	   listsinceblock 	[blockhash] [target-confirmations]

{
    "transactions" : [
        {
            "account" : "testing",
            "address" : "mxxEy4ryr3R3f3EXXGKd2w8z4sWnsZMcb9",
            "category" : "receive",
            "amount" : 10.00000000,
            "confirmations" : 137,
            "blockhash" : "0000000000d11ab706ffce64cb29f54a1d51ba93de3df6c9c645a514e6424d47",
            "blockindex" : 1,
            "blocktime" : 1384856232,
            "txid" : "3e92a4eb30abe2fc99e7c3ae6b908614bea321ced1eae20261d9f6285763a2b4",
            "time" : 1384856232,
            "timereceived" : 1384856942
        }
    ],
    "lastblock" : "0000000000de8cd3feedc9909cad8e2bf071daa02eafefc41f4c396a048b218e"
}

getrawtransaction 3e92a4eb30abe2fc99e7c3ae6b908614bea321ced1eae20261d9f6285763a2b4
0100000001362ba4199d822ed6bc5a44885f85f0053a1ebd2582da59b1e7980f666478a3b2010000006b483045022100ffb3a0d23e76b88b137153578ce39ff24b3b0a74970457a88bdf94365f9403f102206a321767d1448e8df2cd79a12706ffd00d784a7223cfbbd53ad16e720567e7410121033d04604f326951ea0f78b8b6d043dd1cff7777bcc01171df776a20011accb9a7ffffffff02802feeec010000001976a91434a0a7ba1c9d03225cdd7151c8d97fa925b610ae88ac00ca9a3b000000001976a914bf42747a54fa333f398e0046c005b6e6eeb1e45088ac00000000

decoderawtransaction 0100000001362ba4199d822ed6bc5a44885f85f0053a1ebd2582da59b1e7980f666478a3b2010000006b483045022100ffb3a0d23e76b88b137153578ce39ff24b3b0a74970457a88bdf94365f9403f102206a321767d1448e8df2cd79a12706ffd00d784a7223cfbbd53ad16e720567e7410121033d04604f326951ea0f78b8b6d043dd1cff7777bcc01171df776a20011accb9a7ffffffff02802feeec010000001976a91434a0a7ba1c9d03225cdd7151c8d97fa925b610ae88ac00ca9a3b000000001976a914bf42747a54fa333f398e0046c005b6e6eeb1e45088ac00000000

{
    "txid" : "3e92a4eb30abe2fc99e7c3ae6b908614bea321ced1eae20261d9f6285763a2b4",
    "version" : 1,
    "locktime" : 0,
    "vin" : [
        {
            "txid" : "b2a37864660f98e7b159da8225bd1e3a05f0855f88445abcd62e829d19a42b36",
            "vout" : 1,
            "scriptSig" : {
                "asm" : "3045022100ffb3a0d23e76b88b137153578ce39ff24b3b0a74970457a88bdf94365f9403f102206a321767d1448e8df2cd79a12706ffd00d784a7223cfbbd53ad16e720567e74101 033d04604f326951ea0f78b8b6d043dd1cff7777bcc01171df776a20011accb9a7",
                "hex" : "483045022100ffb3a0d23e76b88b137153578ce39ff24b3b0a74970457a88bdf94365f9403f102206a321767d1448e8df2cd79a12706ffd00d784a7223cfbbd53ad16e720567e7410121033d04604f326951ea0f78b8b6d043dd1cff7777bcc01171df776a20011accb9a7"
            },
            "sequence" : 4294967295
        }
    ],
    "vout" : [
        {
            "value" : 82.70000000,
            "n" : 0,
            "scriptPubKey" : {
                "asm" : "OP_DUP OP_HASH160 34a0a7ba1c9d03225cdd7151c8d97fa925b610ae OP_EQUALVERIFY OP_CHECKSIG",
                "hex" : "76a91434a0a7ba1c9d03225cdd7151c8d97fa925b610ae88ac",
                "reqSigs" : 1,
                "type" : "pubkeyhash",
                "addresses" : [
                    "mkKDtYgXqH23PPEGrSGuiUdiJjTKgRUoBR"
                ]
            }
        },
        {
            "value" : 10.00000000,
            "n" : 1,
            "scriptPubKey" : {
                "asm" : "OP_DUP OP_HASH160 bf42747a54fa333f398e0046c005b6e6eeb1e450 OP_EQUALVERIFY OP_CHECKSIG",
                "hex" : "76a914bf42747a54fa333f398e0046c005b6e6eeb1e45088ac",
                "reqSigs" : 1,
                "type" : "pubkeyhash",
                "addresses" : [
                    "mxxEy4ryr3R3f3EXXGKd2w8z4sWnsZMcb9"
                ]
            }
        }
    ]
}
*/

static bool get_lock(int fd)
{
	struct flock fl;
	int ret;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	/* Note: non-blocking! */
	do {
		ret = fcntl(fd, F_SETLK, &fl);
	} while (ret == -1 && errno == EINTR);

	return ret == 0;
}

/* gateway-txs file contains the transactions we've already injected. */
static int read_gateway_txs(struct thash *thash)
{
	struct protocol_double_sha *sha;
	struct stat st;
	int fd, i, num;

	fd = open("gateway-txs", O_RDWR);
	if (fd < 0)
		err(1, "Opening gateway file");

	if (!get_lock(fd))
		err(1, "Locking gateway file");

	if (fstat(fd, &st) == -1)
		err(1, "Statting gateway file");

	if (st.st_size % sizeof(*sha))
		errx(1, "Gateway file invalid size %llu",
		     (long long)st.st_size);

	num = st.st_size / sizeof(*sha);
	sha = tal_arr(NULL, struct protocol_double_sha, num);

	if (read(fd, sha, st.st_size) != st.st_size)
		err(1, "Reading gateway file");

	for (i = 0; i < num; i++)
		thash_add(thash, sha + i);

	return fd;
}

/* arg* may be null */
static char *ask_process(const tal_t *ctx,
			 const char *name,
			 const char *arg1,
			 const char *arg2,
			 const char *arg3,
			 const char *arg4,
			 const char *arg5)
{
	char *output;
	int fds[2], status;

	if (pipe(fds) != 0)
		return NULL;

	switch (fork()) {
	case -1:
		return NULL;
	case 0:
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO);
		execlp(name, name, arg1, arg2, arg3, arg4, arg5, NULL);
		exit(errno);
	}

	close(fds[1]);
	output = grab_fd(ctx, fds[0]);
	close_noerr(fds[0]);

	wait(&status);
	if (!WIFEXITED(status)) {
		output = tal_free(output);
		errno = EINTR;
	} else if (WEXITSTATUS(status)) {
		output = tal_free(output);
		errno = WEXITSTATUS(status);
	}

	return output;
}

static char *ask_bitcoind(const tal_t *ctx,
			  const char *req,
			  const char *opt1,
			  const char *opt2,
			  const char *opt3)
{
	return ask_process(ctx, "bitcoind", "-testnet", req, opt1, opt2, opt3);
}

static jsmntok_t *json_bitcoind(const tal_t *ctx, const char **output,
				const char *req,
				const char *opt1,
				const char *opt2,
				const char *opt3)
{
	bool unused;

	*output = ask_bitcoind(ctx, req, opt1, opt2, opt3);
	if (!*output)
		return NULL;

	/* It should give us valid, complete JSON. */
	return json_parse_input(*output, strlen(*output), &unused);
}

static void setup_gateway(const tal_t *ctx)
{
	jsmntok_t *toks;
	const char *value;
	int fd;

	toks = json_bitcoind(ctx, &value, "getaddressesbyaccount", "gateway",
			     NULL, NULL);
	if (!toks || tal_count(toks) == 1)
		err(1, "Contacting bitcoind for gateway account");

	if (toks[0].type != JSMN_ARRAY)
		errx(1, "Unexpected getaddressesbyaccount answer");

	if (toks[0].size != 0)
		errx(1, "gateway account already exists in bitcoin");

	/* FIXME: Generate our own key? */
	value = ask_bitcoind(ctx, "getaccountaddress", "gateway", NULL, NULL);
	if (!value)
		err(1, "Getting account address for gateway");

	printf("Gateway address is %s\n", value);
	fd = open("gateway-address", O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		err(1, "Creating gateway-address");
	/* Skip \n at the end. */
	if (!write_all(fd, value, strlen(value)-1))
		err(1, "Writing gateway-address");
	close(fd);

	value = ask_bitcoind(ctx, "dumpprivkey", value, NULL, NULL);

	/* Create gateway privkey file */
	fd = open("gateway-privkey", O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		err(1, "Creating gateway-privkey");
	/* Skip \n at the end. */
	if (!write_all(fd, value, strlen(value) - 1))
		err(1, "Writing gateway-privkey");
	fsync(fd);
	close(fd);

	/* Create empty gateway-txs file */
	fd = open("gateway-txs", O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		err(1, "Creating gateway-txs");
	close(fd);
}

/* Tal wrappers for opt. */
static void *opt_allocfn(size_t size)
{
	return tal_alloc_(NULL, size, false, TAL_LABEL("opt_allocfn", ""));
}

static void *tal_reallocfn(void *ptr, size_t size)
{
	if (!ptr)
		return opt_allocfn(size);
	tal_resize_(&ptr, 1, size, false);
	return ptr;
}

static void tal_freefn(void *ptr)
{
	tal_free(ptr);
}

static unsigned long num_of(const char *buffer, const jsmntok_t *tok)
{
	char *end;
	unsigned long val;

	val = strtoul(buffer + tok->start, &end, 0);
	if (end != buffer + tok->end)
		errx(1, "Invalid number value in '%.*s'",
		     json_tok_len(tok),
		     json_tok_contents(buffer, tok));

	return val;
}

static const char *get_first_input_addr(const tal_t *ctx,
					const char *buffer,
					const jsmntok_t *txid)
{
	const jsmntok_t *toks, *intxid, *onum, *type, *address;
	unsigned long outnum;
	const char *delve, *txstr;

	/* eg:
	getrawtransaction e84b0c87934a146d211fcaa6f1ec187da33e205fb250400cf66620a82a9111a0 1
	{
	    "hex" : "010000000193721eadf1bb2a40498b41ca6be2adaddf4c1780884637fedf05cf37015ac508000000006b4830450221008ce700a0c872af67612e5d5226f1e8bdcb2329549883b8fa02df002409f54c43022029b10a885ca3c5103574f06ab90e6f5a7c9c2cafe6e583f17316c967d678bab4012103c511c82cf0aae4541c026eee5d9f738dc1cb4a2114e20b3f5710a22dc94825ceffffffff02d0f88774030000001976a9144e81b0986efad3643012783f016ec10a9c3d35cd88ac005a6202000000001976a91434f89b68a07bd841bbca98f9b7b1521ce1a3d17688ac00000000",
	    "txid" : "e84b0c87934a146d211fcaa6f1ec187da33e205fb250400cf66620a82a9111a0",
	    "version" : 1,
	    "locktime" : 0,
	    "vin" : [
	        {
	            "txid" : "08c55a0137cf05dffe37468880174cdfadade26bca418b49402abbf1ad1e7293",
	            "vout" : 0,
	            "scriptSig" : {
	                "asm" : "30450221008ce700a0c872af67612e5d5226f1e8bdcb2329549883b8fa02df002409f54c43022029b10a885ca3c5103574f06ab90e6f5a7c9c2cafe6e583f17316c967d678bab401 03c511c82cf0aae4541c026eee5d9f738dc1cb4a2114e20b3f5710a22dc94825ce",
	                "hex" : "4830450221008ce700a0c872af67612e5d5226f1e8bdcb2329549883b8fa02df002409f54c43022029b10a885ca3c5103574f06ab90e6f5a7c9c2cafe6e583f17316c967d678bab4012103c511c82cf0aae4541c026eee5d9f738dc1cb4a2114e20b3f5710a22dc94825ce"
	            },
	            "sequence" : 4294967295
	        }
	    ],
	    "vout" : [
	        {
	            "value" : 148.39970000,
	            "n" : 0,
	            "scriptPubKey" : {
	                "asm" : "OP_DUP OP_HASH160 4e81b0986efad3643012783f016ec10a9c3d35cd OP_EQUALVERIFY OP_CHECKSIG",
	                "hex" : "76a9144e81b0986efad3643012783f016ec10a9c3d35cd88ac",
	                "reqSigs" : 1,
	                "type" : "pubkeyhash",
	                "addresses" : [
	                    "mng4N2LZqin7joL7GAjs4tp8WNNt8GjcYz"
	                ]
	            }
	        },
	        {
	            "value" : 0.40000000,
	            "n" : 1,
	            "scriptPubKey" : {
	                "asm" : "OP_DUP OP_HASH160 34f89b68a07bd841bbca98f9b7b1521ce1a3d176 OP_EQUALVERIFY OP_CHECKSIG",
	                "hex" : "76a91434f89b68a07bd841bbca98f9b7b1521ce1a3d17688ac",
	                "reqSigs" : 1,
	                "type" : "pubkeyhash",
	                "addresses" : [
	                    "mkM3FYuYoKFiXS3YL99JdJEdPM5HVGpZTj"
	                ]
	            }
	        }
	    ],
	    "blockhash" : "0000000022a3a02881f56c24bbe7556eef98b1e29267b4015a4fbfc83bab5735",
	    "confirmations" : 14,
	    "time" : 1406252550,
	    "blocktime" : 1406252550
	}
	*/
	txstr = tal_fmt(ctx, "%.*s", 
			(int)(txid->end - txid->start),
			buffer + txid->start);
	toks = json_bitcoind(ctx, &buffer,
			     "getrawtransaction", txstr, "1", NULL);
	if (!toks)
		err(1, "getrawtransaction of tx %s", txstr);

	/* This can happen in if someone aims their block reward at
	   the gateway. */
	intxid = json_delve(buffer, toks, ".vin[0].txid");
	onum = json_delve(buffer, toks, ".vin[0].vout");
	if (!intxid || !onum)
		errx(1, "Missing txid/vout in '%s'", buffer);

	/* Get this before we replace buffer. */
	outnum = num_of(buffer, onum);

	/* Get details of *that* tx. */
	txstr = tal_fmt(ctx, "%.*s", 
			(int)(intxid->end - intxid->start),
			buffer + intxid->start);

	toks = json_bitcoind(ctx, &buffer,
			     "getrawtransaction", txstr, "1", NULL);
	if (!toks)
		err(1, "getrawtransaction of input tx %s", txstr);

	/* make sure it's a pubkeyhash */
	delve = tal_fmt(ctx, ".vout[%lu].scriptPubKey.type", outnum);
	type = json_delve(buffer, toks, delve);
	if (!type || !json_tok_streq(buffer, type, "pubkeyhash"))
		errx(1, "'%s' was not a pubkeyhash in '%s'", delve, buffer);

	delve = tal_fmt(ctx, ".vout[%lu].scriptPubKey.addresses[0]", outnum);
	address = json_delve(buffer, toks, delve);
	if (!address)
		errx(1, "Can't find %s in '%s'", delve, buffer);

	return tal_strndup(ctx, buffer + address->start,
			   address->end - address->start);
}

/* See "https://en.bitcoin.it/wiki/Proper_Money_Handling_(JSON-RPC)" */
static bool pettycoin_tx(const tal_t *ctx, const char *privkey,
			 const char *destaddr, const char *amount,
			 size_t amount_len)
{
	char *out, *end, *amountstr, *pettyaddr;
	u64 amt;

	/* We expect <number>.<number>. */
	amt = strtoul(amount, &end, 10) * (u64)100000000;
	if (end >= amount + amount_len || *end != '.')
		errx(1, "Bad amount '%.*s'", (int)amount_len, amount);

	amt += strtoul(end + 1, &end, 10);
	if (end != amount + amount_len)
		errx(1, "Bad amount '%.*s'", (int)amount_len, amount);

	amountstr = tal_fmt(ctx, "%llu", (unsigned long long)amt);
	pettyaddr = tal_fmt(ctx, "P-%s", destaddr);

	out = ask_process(ctx, "pettycoin-tx", "--no-fee",
			  "from-gateway", privkey, pettyaddr, amountstr);

	if (!out)
		return false;

	out = ask_process(ctx, "pettycoin-query", "sendrawtransaction",
			  out, NULL, NULL, NULL);
	if (!out)
		return false;	

	printf("Injected gateway tx %s\n", out);
	return true;
}

int main(int argc, char *argv[])
{
	tal_t *ctx = tal(NULL, char);
	const char *privkey;
	char *pettycoin_dir, *rpc_filename;
	bool setup = false;
	struct thash thash;
	int fd;
	unsigned int known_txs = 0;

	err_set_progname(argv[0]);

	opt_set_alloc(opt_allocfn, tal_reallocfn, tal_freefn);
	pettycoin_dir_register_opts(ctx, &pettycoin_dir, &rpc_filename);

	opt_register_noarg("--help|-h", opt_usage_and_exit,
			   "", "Show this message");
	opt_register_noarg("--version|-V", opt_version_and_exit, VERSION,
			   "Display version and exit");
	opt_register_noarg("--setup", opt_set_bool, &setup,
			   "Create new gateway key and initialize files");

	opt_early_parse(argc, argv, opt_log_stderr_exit);
	opt_parse(&argc, argv, opt_log_stderr_exit);

	if (argc != 1)
		errx(1, "No arguments needed\n%s", opt_usage(argv[0], NULL));

	if (chdir(pettycoin_dir) != 0)
		err(1, "Entering pettycoin dir '%s'", pettycoin_dir);

	if (setup) {
		setup_gateway(ctx);
		tal_free(ctx);
		exit(0);
	}

	thash_init(&thash);
	fd = read_gateway_txs(&thash);
	privkey = grab_file(ctx, "gateway-privkey");
	if (!privkey)
		err(1, "Reading gateway-privkey");

	for (;;) {
		jsmntok_t *toks;
		const jsmntok_t *t, *end;
		const char *buffer;
		const tal_t *this_ctx = tal(ctx, char);
		char *fromstr = tal_fmt(this_ctx, "%u", known_txs);

		toks = json_bitcoind(this_ctx, &buffer,
				     "listtransactions", "gateway",
				     "999999", fromstr);

		if (!toks)
			err(1, "Asking bitcoind for transactions");

		/* eg:
		   [
		   {
		   "account" : "foo",
		   "address" : "mkM3FYuYoKFiXS3YL99JdJEdPM5HVGpZTj",
		   "category" : "receive",
		   "amount" : 0.40000000,
		   "confirmations" : 0,
		   "txid" : "e84b0c87934a146d211fcaa6f1ec187da33e205fb250400cf66620a82a9111a0",
		   "walletconflicts" : [
		   ],
		   "time" : 1406249415,
		   "timereceived" : 1406249415
		   }
		   ]
		*/
		if (toks[0].type != JSMN_ARRAY)
			errx(1, "Unexpected type from listtransactions!");

		end = json_next(&toks[0]);
		for (t = toks + 1; t < end; t = json_next(t)) {
			const jsmntok_t *val, *amount;
			const char *destaddr;
			struct protocol_double_sha txid;

			if (t->type != JSMN_OBJECT)
				errx(1, "Unexpected type in"
				     " listtransactions array");

			/* Not enough confirmations?  Stop here. */
			val = json_get_member(buffer, t, "confirmations");
			if (num_of(buffer, val) < REQUIRED_CONFIRMATIONS)
				break;

			/* Next time, don't need this tx. */
			known_txs++;

			val = json_get_member(buffer, t, "txid");
			if (!val)
				errx(1, "No txid in '%s'", buffer);

			if (!from_hex(buffer + val->start,
				      val->end - val->start, &txid.sha,
				      sizeof(txid.sha)))
				errx(1, "Bad txid '%.*s'",
				     json_tok_len(val),
				     json_tok_contents(buffer, val));

			/* Do we already know it? */
			if (thash_get(&thash, &txid))
				continue;

			amount = json_get_member(buffer, t, "amount");
			if (!amount)
				errx(1, "No amount in '%s'", buffer);

			/* Get address of first input. */
			destaddr = get_first_input_addr(this_ctx, buffer, val);

			/* OK, inject new from_gateway tx. */
			if (!pettycoin_tx(ctx, privkey, destaddr,
					  buffer + amount->start,
					  amount->end - amount->start)) {
				err(1, "Pettcoin injection failed for"
				     " from-gateway %s P-%s %.*s",
				     privkey, destaddr, 
				     json_tok_len(amount),
				     json_tok_contents(buffer, amount));
			}

			/* FIXME: Racy as hell, we should look on the
			 * pettycoin network to see if we've actually
			 * done all the transfers. */
			if (!write_all(fd, &txid, sizeof(txid)))
				err(1, "Writing out txid");

		}
		/* Clean up. */
		tal_free(this_ctx);

		/* FIXME: Ask pettycoin about TO_GATEWAY txs. */

		/* Poll.  We suck. */
		sleep(5);
	}
}
