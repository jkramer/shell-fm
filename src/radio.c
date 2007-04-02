/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - interface.c
	Copyright (C) 2006 by Jonas Kramer
	Copyright (C) 2006 by Bart Trojanowski <bart@jukie.net>

	Published under the terms of the GNU General Public License (GPLv2).
*/

#define _GNU_SOURCE

#include <config.h>

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <stdarg.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "compatibility.h"
#include "service.h"
#include "interface.h"
#include "http.h"
#include "settings.h"
#include "rl_completion.h"
#include "strarray.h"
#include "md5.h"

#define LASTFM_URL_PREFIX "lastfm://"
#define LASTFM_URL_MIN_LENGTH 6 // shortest possible: 'user/a'

static int init_url_completion (void);
static char ** url_completion (const char *text, int start, int end);
static const char * validate_url (const char *url);


/**
 * this function is called to change the station
 * the user is prompted for the new radio station
 *
 * something like:
 *
 * lastfm://user/BartTrojanowski/loved
 * lastfm://user/BartTrojanowski/personal
 * lastfm://user/BartTrojanowski/neighbour
 * lastfm://user/BartTrojanowski/recommended/100
 * lastfm://usertags/BartTrojanowski/trance
 * lastfm://artist/QED/similarartists
 * lastfm://artist/QED/fans
 * lastfm://globaltags/goa
 * lastfm://globaltags/classical,miles davis,whatever
 * lastfm://multipleartists/QED,Chicane
 * lastfm://play/tracks/########[,#####, ...]
 */
void radioprompt(const char * prompt) {
	char * url;
	const char * url_error;
	char * decoded = NULL;
	unsigned urllen;
	struct rl_params save_rlp;

	save_rl_params(& save_rlp);

	clear_history();
	read_history(rcpath("radio-history"));

	rl_basic_word_break_characters = "/";
	rl_completer_word_break_characters = "/";
	rl_completion_append_character = '/';
	
	rl_startup_hook = init_url_completion;
	rl_attempted_completion_function = url_completion;

	canon(!0);
	url = readline(prompt);
	canon(0);

	if(!url)
		goto bail;

	urllen = strlen(url);
	if(urllen <= 1)
		goto bail;

	// tab completion will add an extra space at the end
	while (isspace (url[urllen-1]))
		url[--urllen] = 0;

	url_error = validate_url (url);
	if (url_error) {
		canon(!0);
		fprintf (stderr, "BAD INPUT: %s\n", url_error);
		canon(0);
		goto bail;
	}

	decode(url, & decoded);
	add_history(url);
	station(decoded);
	free(decoded);

bail:
	free(url);
	restore_rl_params(&save_rlp);
}

/**
 * validate the url, returns NULL on success or error string
 */
static const char * validate_url (const char *url) {
	unsigned url_len = strlen (url);

	if(url_len < LASTFM_URL_MIN_LENGTH)
		return "URL is too short.";

	return NULL;
}

/**
 * this function is called to initialize the readline buffer
 */
static int init_url_completion(void) {
	rl_insert_text("");
	return 0;
}

/* ------------------------------------------------------------------------ */

static char** gen_user_names(time_t *new_expie_time);
static char** gen_user_tag_names(time_t *new_expie_time);
static char** gen_artist_names(time_t *new_expie_time);
static char** gen_tags_names(time_t *new_expie_time);

/* ------------------------------------------------------------------------ */

/*
 * lastfm://user/BartTrojanowski/loved
 * lastfm://user/BartTrojanowski/personal
 * lastfm://user/BartTrojanowski/neighbour
 * lastfm://user/BartTrojanowski/recommended/100
 */
static cmpl_node_t url_user_name_recommended_node[] = {
	{
		.name = "25",
		.gen_word_list = NULL,
	},
	{
		.name = "50",
		.gen_word_list = NULL,
	},
	{
		.name = "75",
		.gen_word_list = NULL,
	},
	{
		.name = "100",
		.gen_word_list = NULL,
	},
	URL_WORD_NODE_END
};
static cmpl_node_t url_user_name_node[] = {
	{
		.name = "loved",
		.gen_word_list = NULL,
	},
	{
		.name = "personal",
		.gen_word_list = NULL,
	},
	{
		.name = "neighbour",
		.gen_word_list = NULL,
	},
	{
		.name = "recommended",
		.gen_word_list = NULL,
		.sub_nodes = url_user_name_recommended_node,
	},
	URL_WORD_NODE_END
};
static cmpl_node_t url_user_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_user_names,
		.sub_nodes = url_user_name_node,
	},
	URL_WORD_NODE_END
};

/*
 * lastfm://usertags/BartTrojanowski/trance
 */
static cmpl_node_t url_usertags_name_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_user_tag_names,
	},
	URL_WORD_NODE_END
};
static cmpl_node_t url_usertags_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_user_names,
		.sub_nodes = url_usertags_name_node,
	},
	URL_WORD_NODE_END
};

/*
 * lastfm://artist/QED/similarartists
 * lastfm://artist/QED/fans
 */
static cmpl_node_t url_artist_name_node[] = {
	{
		.name = "similarartists",
		.gen_word_list = NULL,
	},
	{
		.name = "fans",
		.gen_word_list = NULL,
	},
	URL_WORD_NODE_END
};
static cmpl_node_t url_artist_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_artist_names,
		.sub_nodes = url_artist_name_node,
	},
	URL_WORD_NODE_END
};

/*
 * lastfm://globaltags/goa
 * lastfm://globaltags/classical,miles davis,whatever
 */
static cmpl_node_t url_globaltags_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_tags_names,
		.sub_nodes = NULL, /* url_globaltags_name_node, */
	},
	URL_WORD_NODE_END
};

/*
 * lastfm://globaltags/QED
 * lastfm://globaltags/QED,Chicane
 */
static cmpl_node_t url_multipleartists_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_artist_names,
		.sub_nodes = NULL, /* url_multipleartists_node, */
	},
	URL_WORD_NODE_END
};

/*
 * lastfm://play/tracks/########[,#####, ...]
 */
static cmpl_node_t url_play_node[] = {
	{
		.name = "tracks",
		.gen_word_list = NULL,
		.sub_nodes = NULL, /* url_play_tracks_node */
	},
	URL_WORD_NODE_END
};

static cmpl_node_t url_word_nodes[] = {
	{
		.name = "user",
		.gen_word_list = NULL,
		.sub_nodes = url_user_node,
	},
	{
		.name = "usertags",
		.gen_word_list = NULL,
		.sub_nodes = url_usertags_node,
	},
	{
		.name = "artist",
		.gen_word_list = NULL,
		.sub_nodes = url_artist_node,
	},
	{
		.name = "globaltags",
		.gen_word_list = NULL,
		.sub_nodes = url_globaltags_node,
	},
	{
		.name = "multipleartists",
		.gen_word_list = NULL,
		.sub_nodes = url_multipleartists_node,
	},
	{
		.name = "play",
		.gen_word_list = NULL,
		.sub_nodes = url_play_node,
	},
	URL_WORD_NODE_END
};

/* ------------------------------------------------------------------------ */

/**
 * this function is called for each <TAB> press by readline
 */
static char **url_completion(const char * text, int start, int end) {
	return cmpl_complete (text, start, end, url_word_nodes);
}

/* ------------------------------------------------------------------------ */

/*
 * fetch http://ws.audioscrobbler.com/1.0/user/.../friends.txt
 * and return as list of words
 */
static char** gen_user_names(time_t *new_expie_time) {
	char *url, *user = NULL, **resp, **names;
	unsigned ulen, n, first, name_count, name_idx;

	url = calloc (512, sizeof(char));

	encode(value(&rc, "username"), &user);

	ulen = snprintf (url, 512,
			"http://ws.audioscrobbler.com/1.0/user/%s/friends.txt",
			user);

	free (user);

	resp = fetch (url, NULL, NULL, NULL);
	free (url);

	if (!resp)
		return NULL;

	for (n=0; resp[n]; n++)
		if (resp[n][0] == '\r' || ! resp[n][0])
			break;
	first = n+1;

	for (n=first; resp[n]; n++);
	name_count = n;

	names = calloc (name_count+3, sizeof (char*));

	name_idx = 0;
	names[name_idx++] = strdup (value(&rc, "username"));

	for (n=first; resp[n]; n++) {
		names[name_idx++] = resp[n];
	}
	free (resp);

	// success
	*new_expie_time = time(NULL) + CACHE_WORDS_SEC;
	return names;
}

/*
 * fetch http://ws.audioscrobbler.com/1.0/user/.../tags.txt
 * and return as list of words
 */
static char** gen_user_tag_names(time_t *new_expie_time) {
	char *url, *user = NULL, **resp;
	unsigned ulen, t, first, u;

	url = calloc (512, sizeof(char));

	user = cmpl_state.url_sa.strings[cmpl_state.word_idx-1];

	ulen = snprintf (url, 512,
			"http://ws.audioscrobbler.com/1.0/user/%s/tags.txt",
			user);

	resp = fetch (url, NULL, NULL, NULL);
	free (url);

	if (!resp)
		return NULL;

	for (t=0; resp[t]; t++)
		if (resp[t][0] == '\r' || ! resp[t][0])
			break;
	first = t+1;

	for (t=first, u=0; resp[t]; t++) {
		char *s, *e, *n;

		// format: count,name,url
		// but we just wnat the name

		s = strchr (resp[t], ',');
		if (!s) continue;
		s++;

		e = strchr (s, ',');
		if (!e) continue;

		n = strndup (s, e-s);
		free (resp[u]);
		resp[u++] = n;
	}
	resp[u++] = NULL;

	while (resp[u])
		free (resp[u++]);

	// success
	*new_expie_time = time(NULL) + CACHE_WORDS_SEC;
	return resp;
}

/*
 * fetch http://ws.audioscrobbler.com/1.0/user/.../topartists.txt
 * and return as list of words
 */
static char** gen_artist_names(time_t *new_expie_time) {
	char *url, *user = NULL, **resp;
	unsigned ulen, t, first, u;

	url = calloc (512, sizeof(char));

	encode(value(&rc, "username"), &user);

	ulen = snprintf (url, 512,
			"http://ws.audioscrobbler.com/1.0/user/%s/topartists.txt",
			user);

	free (user);

	resp = fetch (url, NULL, NULL, NULL);
	free (url);

	if (!resp)
		return NULL;

	for (t=0; resp[t]; t++)
		if (resp[t][0] == '\r' || ! resp[t][0])
			break;
	first = t+1;

	for (t=first, u=0; resp[t]; t++) {
		char *s, *n;

		// format: ???,count,artist
		// but we just wnat the artist

		s = strchr (resp[t], ',');
		if (!s) continue;
		s++;

		s = strchr (s, ',');
		if (!s) continue;
		s++;
		
		n = strdup (s);
		free (resp[u]);
		resp[u++] = n;
	}
	resp[u++] = NULL;

	while (resp[u])
		free (resp[u++]);

	// success
	*new_expie_time = time(NULL) + CACHE_WORDS_SEC;
	return resp;
}

/*
 * fetch http://ws.audioscrobbler.com/1.0/user/.../tags.txt
 * and return as list of words
 */
static char** gen_tags_names(time_t *new_expie_time) {
	char *url, *user = NULL, **resp;
	unsigned ulen, t, first, u;

	url = calloc (512, sizeof(char));

	encode(value(&rc, "username"), &user);

	ulen = snprintf (url, 512,
			"http://ws.audioscrobbler.com/1.0/user/%s/tags.txt",
			user);

	free (user);

	resp = fetch (url, NULL, NULL, NULL);
	free (url);

	if (!resp)
		return NULL;

	for (t=0; resp[t]; t++)
		if (resp[t][0] == '\r' || ! resp[t][0])
			break;
	first = t+1;

	for (t=first, u=0; resp[t]; t++) {
		char *s, *e, *n;

		// format: count,tag,url
		// but we just wnat the tag

		s = strchr (resp[t], ',');
		if (!s) continue;
		s++;

		e = strchr (s, ',');
		if (!e) continue;

		n = strndup (s, e-s);
		free (resp[u]);
		resp[u++] = n;
	}
	resp[u++] = NULL;

	while (resp[u])
		free (resp[u++]);

	// success
	*new_expie_time = time(NULL) + CACHE_WORDS_SEC;
	return resp;
}


