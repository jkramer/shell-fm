
#include <stdlib.h>

#include "select.h"

#include "interface.h"
#include "sckif.h"
#include "util.h"

static struct input * input_list = NULL;
static int input_list_size = 0;


void register_handle(struct input handle) {
	input_list = realloc(input_list, sizeof(struct input) * (input_list_size + 1));
	input_list[input_list_size] = handle;
	input_list_size++;

	debug("registered handle %i of type %i\n", handle.handle, handle.type);
}

void remove_handle(int handle) {
	int i = input_list_size;

	while(i-- > 0) {
		if(input_list[i].handle == handle) {
			input_list[i] = input_list[input_list_size - 1];
			input_list = realloc(input_list, (--input_list_size) * sizeof(struct input));

			debug("removing handle: %i\n", handle);

			break;
		}
	}
}

struct input * select_input(unsigned nsec) {
	fd_set fdset;
	struct timeval tv;
	int highest = 0, i = input_list_size;

	FD_ZERO(& fdset);

	while(i-- > 0) {
		int handle = input_list[i].handle;

		if(highest < handle)
			highest = handle;

		FD_SET(handle, & fdset);
	}


	tv.tv_usec = nsec % 1000000;
	tv.tv_sec = nsec / 1000000;

	if(select(highest + 1, & fdset, NULL, NULL, & tv) > 0) {
		i = input_list_size;

		while(i-- > 0) {
			int handle = input_list[i].handle;

			if(FD_ISSET(handle, & fdset))
				return & input_list[i];
		}
	}

	return NULL;
}

void handle_input(unsigned nsec) {
	struct input * input = select_input(nsec);

	if(input != NULL) {
		debug("input ready: fd %i, type %i\n", input->handle, input->type);

		switch(input->type) {
			case KEYBOARD:
				handle_keyboard_input();
				break;

			case LISTEN_SOCKET:
				accept_client(input->handle);
				break;

			case CLIENT_SOCKET:
				handle_client(input->handle);
				break;
		}
	}
}

void register_listen_socket(int handle) {
	struct input socket_input = { handle, LISTEN_SOCKET };
	register_handle(socket_input);
}

void register_client_socket(int handle) {
	struct input socket_input = { handle, CLIENT_SOCKET };
	register_handle(socket_input);
}
