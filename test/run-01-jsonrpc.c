#include <stdio.h>
#include <stdarg.h>
#include <ccan/io/io.h>
#include <ccan/tal/tal.h>

#include "../jsonrpc.c"
#include "../json.c"
#include "../minimal_log.c"

/* AUTOGENERATED MOCKS START */
/* Generated stub for getinfo_command */
struct json_command getinfo_command;
/* Generated stub for listtodo_command */
struct json_command listtodo_command;
/* Generated stub for listtransactions_command */
struct json_command listtransactions_command;
/* Generated stub for pettycoin_to_base58 */
char *pettycoin_to_base58(const tal_t *ctx, bool test_net,
			  const struct protocol_address *addr,
			  bool bitcoin_style)
{ fprintf(stderr, "pettycoin_to_base58 called!\n"); abort(); }
/* Generated stub for sendrawtransaction_command */
struct json_command sendrawtransaction_command;
/* Generated stub for to_hex */
char *to_hex(const tal_t *ctx, const void *buf, size_t bufsize)
{ fprintf(stderr, "to_hex called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

void test(const char *input, const char *expect, bool needs_more, int extra)
{
	struct state state;
	struct json_connection *jcon = tal(NULL, struct json_connection);
	struct io_plan plan;

	jcon->used = 0;
	jcon->len_read = strlen(input);
	jcon->buffer = tal_dup(jcon, char, input, strlen(input), 0);
	jcon->state = &state;
	jcon->num_conns = 2;
	list_head_init(&jcon->output);

	plan = read_json(NULL, jcon);
	if (needs_more) {
		/* Should have done partial read for rest. */
		assert(jcon->used == strlen(input));
		assert(plan.next == (void *)read_json);
		assert(plan.u1.cp == jcon->buffer + strlen(input));
		assert(list_empty(&jcon->output));
	} else if (!expect) {
		/* Should have returned io_close. */
		assert(plan.next == NULL);
	} else {
		/* Should have finished. */
		assert(jcon->used == extra);
		assert(plan.next == (void *)read_json);
		assert(!list_empty(&jcon->output));
		assert(streq(list_top(&jcon->output, struct json_output, list)
			     ->json, expect));
	}
	tal_free(jcon);
}	

int main(void)
{
	unsigned int i;
	const char *cmd;
	const char echocmd[] = "{ \"method\" : \"dev-echo\", "
		"\"params\" : [ \"hello\", \"Arabella!\" ], "
		"\"id\" : \"1\" }";
	const char echoresult[]
		= "{ \"result\" : { \"num\" : 2,"
		" \"echo\" : [ \"hello\", \"Arabella!\" ] }, "
		"\"error\" : null, \"id\" : \"1\" }\n";

	/* Make partial commands work. */
	for (i = 1; i < strlen(echocmd); i++) {
		cmd = tal_strndup(NULL, echocmd, i);
		test(cmd, NULL, true, 0);
		tal_free(cmd);
	}

	test(echocmd, echoresult, false, 0);

	/* Two commands at once will also work (second will be left in buf) */
	cmd = tal_fmt(NULL, "%s%s", echocmd, echocmd);

	test(cmd, echoresult, false, strlen(echocmd));
	tal_free(cmd);

	/* Unknown method. */
	test("{ \"method\" : \"unknown\", "
	     "\"params\" : [ \"hello\", \"Arabella!\" ], "
	     "\"id\" : \"2\" }",
	     "{ \"result\" : null, "
	     "\"error\" : \"Unknown command 'unknown'\", \"id\" : \"2\" }\n",
	     false, 0);

	/* Missing parts, will fail. */
	test("{ \"params\" : [ \"hello\", \"Arabella!\" ], "
	     "\"id\" : \"2\" }", NULL, false, 0);
	test("{ \"method\" : \"echo\", "
	     "\"id\" : \"2\" }", NULL, false, 0);
	test("{ \"method\" : \"echo\", "
	     "\"params\" : [ \"hello\", \"Arabella!\" ] }", NULL, false, 0);

	/* It doesn't help to have them in successive commands. */
	test("{ \"params\" : [ \"hello\", \"Arabella!\" ], "
	     "\"id\" : \"2\" }"
	     "{ \"method\" : \"unknown\", "
	     "\"params\" : [ \"hello\", \"Arabella!\" ], "
	     "\"id\" : \"2\" }", NULL, false, 0);

	return 0;
}
