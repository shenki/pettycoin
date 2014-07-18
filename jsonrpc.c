/* Code for JSON_RPC API */
/* eg: { "method" : "echo", "params" : [ "hello", "Arabella!" ], "id" : "1" } */
#include "json.h"
#include "jsonrpc.h"
#include "log.h"
#include "state.h"
#include <ccan/array_size/array_size.h>
#include <ccan/err/err.h>
#include <ccan/io/io.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/tal/str/str.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

struct json_buf {
	struct state *state;
	int infd, outfd;
	/* How much is already filled. */
	size_t used;
	/* How much has just been filled. */
	size_t len;
	char *buffer;
};

static void free_buf(struct io_conn *conn, struct json_buf *buf)
{
	log_info(buf->state->log, "Closing json input (%s)", strerror(errno));
	tal_free(buf);
}

struct command {
	const char *name;
	bool (*dispatch)(const struct json_buf *buf,
			 const jsmntok_t *params,
			 const jsmntok_t *id);
};

static void write_str(int fd, const char *str)
{
	write_all(fd, str, strlen(str));
}

/* FIXME: Async! */
static void json_send(const struct json_buf *buf,
		      const char *result,
		      const char *error,
		      const jsmntok_t *id)
{
	write_str(buf->outfd, "{ \"result\" : ");
	write_str(buf->outfd, result);
	write_str(buf->outfd, ", \"error\" : ");
	write_str(buf->outfd, error);
	write_str(buf->outfd, ", \"id\" : ");
	write_all(buf->outfd,
		  json_tok_contents(buf->buffer, id),
		  json_tok_len(id));
	write_str(buf->outfd, " }\n");
}

static void json_respond(const struct json_buf *buf, const jsmntok_t *id,
			 const char *fmt, ...)
{
	va_list ap;
	char *result;

	va_start(ap, fmt);
	result = tal_vfmt(buf, fmt, ap);
	json_send(buf, result, "null", id);
	tal_free(result);
	va_end(ap);
}

/* FIXME: async! */
static void json_err(const struct json_buf *buf, const jsmntok_t *id,
		     const char *fmt, ...)
{
	va_list ap;
	char *error, *error_str;

	va_start(ap, fmt);
	error = tal_vfmt(buf, fmt, ap);
	error_str = tal_fmt(error, "\"%s\"", error);
	json_send(buf, "null", error_str, id);
	tal_free(error);
	va_end(ap);
}

static bool json_help(const struct json_buf *buf,
		      const jsmntok_t *params,
		      const jsmntok_t *id)
{
	json_respond(buf, id, "\"Not very helpful, I'm afraid!\"");
	return true;
}

static bool json_echo(const struct json_buf *buf,
		      const jsmntok_t *params,
		      const jsmntok_t *id)
{
	json_respond(buf, id, "{ \"num\" : %u, %.*s }",
		     params->size,
		     json_tok_len(params),
		     json_tok_contents(buf->buffer, params));
	return true;
}

static const struct command cmdlist[] = {
	{ "help", json_help },
	{ "echo", json_echo }
};

static const struct command *find_cmd(const char *buffer, const jsmntok_t *tok)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(cmdlist); i++)
		if (json_tok_streq(buffer, tok, cmdlist[i].name))
			return &cmdlist[i];
	return NULL;
}

/* Returns false if it's a fatal error. */
static bool parse_request(struct json_buf *buf, const jsmntok_t tok[])
{
	const jsmntok_t *method, *id, *params, *t;
	const struct command *cmd;

	if (tok[0].type != JSMN_OBJECT) {
		log_unusual(buf->state->log, "Expected {} for json command");
		return false;
	}

	method = json_get_label(buf->buffer, tok, "method");
	params = json_get_label(buf->buffer, tok, "params");
	id = json_get_label(buf->buffer, tok, "id");

	if (!id || !method || !params) {
		log_unusual(buf->state->log, "json: No %s",
			    !id ? "id" : (!method ? "method" : "params"));
		return false;
	}

	id++;
	if (id->type != JSMN_STRING && id->type != JSMN_PRIMITIVE) {
		log_unusual(buf->state->log,
			    "Expected string/primitive for id");
		return false;
	}

	t = method + 1;
	if (t->type != JSMN_STRING) {
		log_unusual(buf->state->log, "Expected string for method");
		return false;
	}

	cmd = find_cmd(buf->buffer, t);
	if (!cmd) {
		json_err(buf, id, "Unknown command '%.*s'",
			 (int)(t->end - t->start), buf->buffer + t->start);
		return true;
	}

	t = params + 1;
	if (t->type != JSMN_ARRAY) {
		log_unusual(buf->state->log, "Expected array after params");
		return false;
	}

	return cmd->dispatch(buf, t, id);
}

static struct io_plan read_json(struct io_conn *conn, struct json_buf *buf)
{
	jsmntok_t *toks;
	bool valid;

	/* Resize larger if we're full. */
	buf->used += buf->len;
	if (buf->used == tal_count(buf->buffer))
		tal_resize(&buf->buffer, buf->used * 2);

again:
	toks = json_parse_input(buf->buffer, buf->used, &valid);
	if (!toks) {
		if (!valid) {
			log_unusual(buf->state->log,
				    "Invalid token in json input: '%.*s'",
				    (int)buf->used, buf->buffer);
			return io_close();
		}
		/* We need more. */
		buf->len = tal_count(buf->buffer) - buf->used;
		return io_read_partial(buf->buffer + buf->used,
				       &buf->len, read_json, buf);
	}

	/* Empty buffer? (eg. just whitespace). */
	if (tal_count(toks) == 0) {
		tal_free(toks);
		buf->used = 0;
		return io_read_partial(buf->buffer + buf->used,
				       &buf->len, read_json, buf);
	}

	if (!parse_request(buf, toks))
		return io_close();

	/* Remove first {}, retry. */
	memmove(buf->buffer, buf->buffer + toks[0].end,
		tal_count(buf->buffer) - toks[0].end);
	buf->used -= toks[0].end;
	tal_free(toks);
	goto again;
}

static void init_rpc(int infd, int outfd, struct state *state)
{
	struct json_buf *buf;
	struct io_conn *conn;

	buf = tal(state, struct json_buf);
	buf->state = state;
	buf->used = 0;
	buf->len = 1;
	buf->buffer = tal_arr(buf, char, buf->len);
	buf->infd = infd;
	buf->outfd = outfd;

	conn = io_new_conn(buf->infd,
			   io_read_partial(buf->buffer, &buf->len,
					   read_json, buf));
	io_set_finish(conn, free_buf, buf);
}

static void rpc_connected(int fd, struct state *state)
{
	log_info(state->log, "Connected json input");
	init_rpc(fd, fd, state);
}

void setup_jsonrpc(struct state *state, const char *rpc_filename)
{
	if (!rpc_filename)
		return;

	if (streq(rpc_filename, "-")) {
		init_rpc(STDIN_FILENO, STDOUT_FILENO, state);
	} else {
		struct sockaddr_un addr;
		int fd, old_umask;

		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (strlen(rpc_filename) + 1 > sizeof(addr.sun_path))
			errx(1, "rpc filename '%s' too long", rpc_filename);
		strcpy(addr.sun_path, rpc_filename);
		addr.sun_family = AF_UNIX;

		/* Of course, this is racy! */
		if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
			errx(1, "rpc filename '%s' in use", rpc_filename);
		unlink(rpc_filename);

		/* This file is only rw by us! */
		old_umask = umask(0177);
		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)))
			err(1, "Binding rpc socket to '%s'", rpc_filename);
		umask(old_umask);

		if (listen(fd, 1) != 0)
			err(1, "Listening on '%s'", rpc_filename);

		io_new_listener(fd, rpc_connected, state);
	}
}
