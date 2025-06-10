

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>

/* Message handling */
#include <sys/msg.h>

/* create_dev() */
#include <posix/utils.h>

#define BUFFER_SIZE 16

struct Buffer
{
	char data[BUFFER_SIZE];
	size_t len;
};

void print_data(char * header, void * data, size_t len)
{
	printf("%s: len=%ld", header, (unsigned long)len);
	for (size_t i = 0; i < len; ++i)
		printf(" %02x", ((unsigned char *)data)[i]);
	printf("\r\n");
}

static int server_handleOpen(oid_t *oid)
{
	/* Just allow open() on our interface.
	 * We need to handle this to allow open()
	 * to work with our server. */
	(void)oid;
	printf("Open oid %u:%u\r\n", (unsigned)oid->port, (unsigned)oid->id);
	return 0;
}

static int server_handleClose(oid_t *oid)
{
	/* Just allow close() on our interface.
	 * We need to handle this to allow close()
	 * to work with our server. */
	(void)oid;
	printf("Close oid %u:%u\r\n", (unsigned)oid->port, (unsigned)oid->id);
	return 0;
}

static ssize_t server_handleRead(oid_t *oid, void *data, size_t len, struct Buffer * buffer)
{
	/* This is where we handle read request (i.e. user is reading from the server). */
	printf("Read from oid %u:%u of %zu bytes\r\n", (unsigned)oid->port, (unsigned)oid->id, len);
//	print_data( "  Read: buffer", buffer->data, buffer->len);
	if( len > buffer->len)
		len = buffer->len;
	if( len > 0)
	{
		memcpy(data, buffer->data, len);
		buffer->len -= len;
		if( buffer->len > 0UL)
			memmove(& buffer->data[0], & buffer->data[len], buffer->len);
	}
	print_data( "  Read: data", data, len);
	print_data( "  Read: buffer", buffer->data, buffer->len);
	/* Actual read length or error (negative value). */
	return (ssize_t)len;
}

static ssize_t server_handleWrite(oid_t *oid, const void *data, size_t len, struct Buffer * buffer)
{
	/* This is where we handle write request (i.e. user is writing to the server). */
	printf("Write to oid %u:%u of %zu bytes\r\n", (unsigned)oid->port, (unsigned)oid->id, len);
	/* Print out received data. */
	print_data( "  Write: data", data, len);
	size_t rotValue = 1;
	const unsigned char minChar = '#';
	const unsigned char maxChar = '~';
	rotValue %= (maxChar - minChar + 1);
	unsigned char currentChar;
	size_t i = 0;
	for(; i < len; ++i)
	{
		currentChar = (unsigned char)((char *)data)[i];
		if( buffer->len == BUFFER_SIZE) {
			break;
		}
		if( currentChar >= minChar && currentChar <= maxChar )
		{
			if (rotValue > (currentChar - minChar)) {
				currentChar -= rotValue + minChar - maxChar - 1;
			}
			else {
				currentChar -= rotValue;
			}
		}
		buffer->data[(buffer->len)++] = currentChar;
	}
	print_data( "  Write: buffer", buffer->data, buffer->len);
	/* Actual write length or error (negative value). */
	return (ssize_t)i;
}


__attribute__((noreturn)) static void server_msgLoop(oid_t *oid)
{
	struct Buffer buffer;
	buffer.len = 0;
	/* This is a message structure. It is filled-in by kernel during
	 * msgRecv() syscall. It contains all data necessary to handle
	 * the request. */
	msg_t msg;
	/* This is a message response ID. It allows the kernel to distinguish
	 * between messages passed to the server when the server is responding.
	 * It is needed, because the server might be handling more than one
	 * message in parallel at any given time, so the kernel needs an
	 * information, to which user the server it responding. */
	msg_rid_t rid;
	for (;;) {
		/* Receive the next message. This call will block until a message
		 * becomes available. It can be interrupted by a posix signal. */
		int err = msgRecv(oid->port, &msg, &rid);
		if (err < 0) {
			if (err == -EINTR) {
				/* We were interrupted by a posix signal. So we
				 * just try again to receive a valid message. */
				continue;
			}
			else {
				/* Some serious error occurred. We end the server
				 * process as we're unable to process messages. */
				fprintf(stderr, "serverDecryption: msgRecv returned %d (%s)\r\n", err, strerror(err));
				exit(EXIT_FAILURE);
			}
		}
		/* We got a message, now we need to process it. */
		switch (msg.type) {
			/* There are many other message types, as defined in
			 * sys/msg.h header. In this example we will handle
			 * only selected types. */
			case mtOpen:
				msg.o.io.err = server_handleOpen(&msg.i.openclose.oid);
				break;

			case mtClose:
				msg.o.io.err = server_handleClose(&msg.i.openclose.oid);
				break;

			case mtRead:
				/* Buffer in msg.o.data is provided by the user and
				 * was allocated in the server memory space by the kernel. */
				msg.o.io.err = server_handleRead(&msg.i.io.oid, msg.o.data, msg.o.size, & buffer);
				break;

			case mtWrite:
				/* Buffer in msg.i.data is provided by the user and
				 * was allocated in the server memory space by the kernel. */
				msg.o.io.err = server_handleWrite(&msg.i.io.oid, msg.i.data, msg.i.size, & buffer);
				break;

			default:
				/* All other types are not supported. */
				msg.o.io.err = -ENOSYS;
				break;
		}
		/* Now we respond to the message we just handled.
		 * We pass the message that we received (modified by
		 * processing it) back to the kernel, along with the
		 * respond ID that we received from msgRecv(). */
		msgRespond(oid->port, &msg, rid);
	}
}

int main(void)
{
	/* Our oid - unique object identifier that consists of:
	 * port - a unique port, assigned by the kernel via portCreate(),
	 *        used by the server to receive messages,
	 * id   - a unique within each server identifier that identifies
	 *        every special file created by the server. */
	oid_t oid;
	/* Assign id to zero, we only have one special file, so it doesn't matter. */
	oid.id = 0;
	/* Create the port */
	if (portCreate(&oid.port) < 0) {
		fprintf(stderr, "serverDecryption: portCreate failed\r\n");
		return EXIT_FAILURE;
	}
	/* Now we need to register our port within the filesystem to allow
	 * other processes to find out its value and to communicate with us.
	 * We use create_dev() to do this. This function creates a special
	 * file in the /dev directory and registers oid to it. This file can
	 * be later used by the user to communicate with the server. */
	if (create_dev(&oid, "serverDecryption") < 0) {
		fprintf(stderr, "serverDecryption: create_dev failed\r\n");
		return EXIT_FAILURE;
	}
	/* We're ready, start receiving and handling messages. */
	server_msgLoop(&oid);
	/* Never reached */
	return EXIT_FAILURE;
}
