
#ifndef SHELLFM_SELECT
#define SHELLFM_SELECT

enum input_type {
	KEYBOARD, LISTEN_SOCKET, CLIENT_SOCKET
};

struct input {
	int handle;
	enum input_type type;
};

void register_handle(struct input);
void handle_input(unsigned);
void register_listen_socket(int);
void register_client_socket(int);
void remove_handle(int);

#endif
