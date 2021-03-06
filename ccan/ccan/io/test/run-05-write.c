#include <ccan/io/io.h>
/* Include the C files directly. */
#include <ccan/io/poll.c>
#include <ccan/io/io.c>
#include <ccan/tap/tap.h>
#include <sys/wait.h>
#include <stdio.h>

#ifndef PORT
#define PORT "65005"
#endif

struct data {
	int state;
	size_t bytes;
	char *buf;
};

static void finish_ok(struct io_conn *conn, struct data *d)
{
	ok1(d->state == 1);
	d->state++;
	io_break(d, io_never());
}

static void init_conn(int fd, struct data *d)
{
	ok1(d->state == 0);
	d->state++;
	io_set_finish(io_new_conn(fd, io_write(d->buf, d->bytes,
					       io_close_cb, d)),
		      finish_ok, d);
}

static int make_listen_fd(const char *port, struct addrinfo **info)
{
	int fd, on = 1;
	struct addrinfo *addrinfo, hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;

	if (getaddrinfo(NULL, port, &hints, &addrinfo) != 0)
		return -1;

	fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
		    addrinfo->ai_protocol);
	if (fd < 0)
		return -1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
		close(fd);
		return -1;
	}
	if (listen(fd, 1) != 0) {
		close(fd);
		return -1;
	}
	*info = addrinfo;
	return fd;
}

static void read_from_socket(size_t bytes, const struct addrinfo *addrinfo)
{
	int fd, done, r;
	char buf[100];

	fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
		    addrinfo->ai_protocol);
	if (fd < 0)
		exit(1);
	if (connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0)
		exit(2);

	for (done = 0; done < bytes; done += r) {
		r = read(fd, buf, sizeof(buf));
		if (r < 0)
			exit(3);
		done += r;
	}
	close(fd);
}

int main(void)
{
	struct data *d = malloc(sizeof(*d));
	struct addrinfo *addrinfo;
	struct io_listener *l;
	int fd, status;

	/* This is how many tests you plan to run */
	plan_tests(9);
	d->state = 0;
	d->bytes = 1024*1024;
	d->buf = malloc(d->bytes);
	memset(d->buf, 'a', d->bytes);
	fd = make_listen_fd(PORT, &addrinfo);
	ok1(fd >= 0);
	l = io_new_listener(fd, init_conn, d);
	ok1(l);
	fflush(stdout);
	if (!fork()) {
		io_close_listener(l);
		read_from_socket(d->bytes, addrinfo);
		freeaddrinfo(addrinfo);
		free(d->buf);
		free(d);
		exit(0);
	}
	ok1(io_loop() == d);
	ok1(d->state == 2);

	ok1(wait(&status));
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	freeaddrinfo(addrinfo);
	free(d->buf);
	free(d);
	io_close_listener(l);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
