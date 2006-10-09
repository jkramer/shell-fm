/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - interface.c
	Copyright (C) 2006 by Jonas Kramer
	Copyright (C) 2006 by Bart Trojanowski <bart@jukie.net>

	Published under the terms of the GNU General Public License (GPLv2).
*/

#define _GNU_SOURCE

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

#include <openssl/md5.h>

#include "compatibility.h"
#include "service.h"
#include "interface.h"
#include "http.h"
#include "settings.h"

#define LASTFM_URL_PREFIX "lastfm://"
#define LASTFM_URL_MIN_LENGTH (strlen(LASTFM_URL_PREFIX)+2)

static int matches_one_of (const char *word, ...) __attribute__((unused));

static int init_url_completion (void);
static char** url_completion (const char *text, int start, int end);
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
 * lastfm://play/tracks/########[,#####, ...]
 */
void radioprompt(const char * prompt) {
	char * url;
	const char *url_error;
	char * decoded = NULL;
	unsigned urllen;

	rl_basic_word_break_characters = "/";
	rl_completer_word_break_characters = "/";
	rl_completion_append_character = '/';
	
	rl_startup_hook = init_url_completion;
	rl_attempted_completion_function = url_completion;

	canon(!0);
	url = readline(prompt);
	canon(0);

	if(!url)
		return;

	urllen = strlen(url);
	if(urllen <= 1)
		goto bail;

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
	add_history(decoded);
	station(decoded);
	free(decoded);

bail:
	free(url);
}

/**
 * validate the url, returns NULL on success or error string
 */
static const char * validate_url (const char *url) {
	unsigned url_len = strlen (url);

	if (url_len < LASTFM_URL_MIN_LENGTH)
		return "url is too short";

	if (strncmp (url, LASTFM_URL_PREFIX, strlen(LASTFM_URL_PREFIX)))
		return "invalid url prefix (lastfm://)";

	return NULL;
}

/**
 * this function is called to initialize the readline buffer
 */
static int init_url_completion(void) {
	rl_insert_text("lastfm://");
	return 0;
}

/* ------------------------------------------------------------------------ */

typedef struct url_word_node_s {
	const char *name;
	char** (*gen_word_list)(time_t *new_expie_time);
	char** cached_list;
	time_t cache_expires;
	struct url_word_node_s *sub_nodes;
} url_word_node_t;
#define URL_WORD_NODE_END { .name = NULL, .gen_word_list = NULL, }
#define CACHE_WORDS_SEC 60

// split the readline buffer into words
static char **get_lastfm_url_words(const char *url);
static void free_lastfm_url_words(char **url_words);

// node utility functions
static url_word_node_t* find_word_in_table(const char *word, url_word_node_t *table);
static void free_node_cached_words (url_word_node_t *node);

// generate words that match currently completed word
typedef struct url_completion_state_s { 
	char **url_words;
	unsigned url_word_count;
	url_word_node_t *node;
	unsigned word_len;
	unsigned list_index;
} url_completion_state_t;
static url_completion_state_t state;
static char *url_nomatch_generator(const char *text, int gen_state);
static char *url_match_generator(const char *text, int gen_state);

static char** gen_user_names(time_t *new_expie_time);
static char** gen_user_tag_names(time_t *new_expie_time);
static char** gen_artist_names(time_t *new_expie_time);
static char** gen_tags_names(time_t *new_expie_time);

static url_word_node_t url_word_nodes[];
static url_word_node_t url_user_node[];
static url_word_node_t url_user_name_node[];
static url_word_node_t url_user_name_recommended_node[];
static url_word_node_t url_usertags_node[];
static url_word_node_t url_usertags_name_node[];
static url_word_node_t url_artist_node[];
static url_word_node_t url_artist_name_node[];
static url_word_node_t url_globaltags_node[];
static url_word_node_t url_play_node[];

/* ------------------------------------------------------------------------ */

static url_word_node_t url_word_nodes[] = {
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
		.name = "play",
		.gen_word_list = NULL,
		.sub_nodes = url_play_node,
	},
	URL_WORD_NODE_END
};

/*
 * lastfm://user/BartTrojanowski/loved
 * lastfm://user/BartTrojanowski/personal
 * lastfm://user/BartTrojanowski/neighbour
 * lastfm://user/BartTrojanowski/recommended/100
 */
static url_word_node_t url_user_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_user_names,
		.sub_nodes = url_user_name_node,
	},
	URL_WORD_NODE_END
};
static url_word_node_t url_user_name_node[] = {
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
static url_word_node_t url_user_name_recommended_node[] = {
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

/*
 * lastfm://usertags/BartTrojanowski/trance
 */
static url_word_node_t url_usertags_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_user_names,
		.sub_nodes = url_usertags_name_node,
	},
	URL_WORD_NODE_END
};
static url_word_node_t url_usertags_name_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_user_tag_names,
	},
	URL_WORD_NODE_END
};

/*
 * lastfm://artist/QED/similarartists
 * lastfm://artist/QED/fans
 */
static url_word_node_t url_artist_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_artist_names,
		.sub_nodes = url_artist_name_node,
	},
	URL_WORD_NODE_END
};
static url_word_node_t url_artist_name_node[] = {
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

/*
 * lastfm://globaltags/goa
 * lastfm://globaltags/classical,miles davis,whatever
 */
static url_word_node_t url_globaltags_node[] = {
	{
		.name = NULL,
		.gen_word_list = gen_tags_names,
		.sub_nodes = NULL, /* url_globaltags_name_node, */
	},
	URL_WORD_NODE_END
};

/*
 * lastfm://play/tracks/########[,#####, ...]
 */
static url_word_node_t url_play_node[] = {
	{
		.name = "tracks",
		.gen_word_list = NULL,
		.sub_nodes = NULL, /* url_play_tracks_node */
	},
	URL_WORD_NODE_END
};

/* ------------------------------------------------------------------------ */

/**
 * this function is called for each <TAB> press by readline
 */
static char **url_completion(const char * text, int start, int __UNUSED__ end) {

	char **ret;
	char **url_words;
	url_word_node_t *table;
	unsigned word_idx;
	char* (*match_generator)(const char*, int);
	
	// if we have nothing, fill in the lastfm://
	if (start == 0) {
		// tell readline to append the /
		rl_completion_append_character = '/';

		// this will generate a lastfm:// on the command line
		ret = calloc (2, sizeof(char*));
		ret[0] = strdup ("lastfm:/");
		return ret;
	}

	url_words = get_lastfm_url_words (rl_line_buffer);
	if (!url_words)
		return NULL;

	// start at the first level
	table = url_word_nodes;
	word_idx = 0;

	while ( (url_words[word_idx+1])
			&& (table = find_word_in_table (url_words[word_idx], table)) ) {
		word_idx ++;
	}

	match_generator = url_nomatch_generator;
	if (table) {
		// we found a place in the tree, use a real match generation function
		match_generator = url_match_generator;

		// we have the table in which we are completing, we need to find the nodes
		// that make the most sense
		state.node = table;
		state.url_words = url_words;
		state.url_word_count = word_idx;

		// we assume it's the last entry, this is updated in url_match_generator()
		rl_completion_append_character = ' ';
	}

	// generate completions that match text
	rl_filename_completion_desired = 0;
	ret = rl_completion_matches(text, url_match_generator);

	free_lastfm_url_words (url_words);

	rl_filename_completion_desired = 0;
	rl_attempted_completion_over = 1;
	return ret;
}

/**
 * this function is called by readline to get each word that matches what is
 * being completed, except this is a decoy that generates no matches
 */
static char *url_nomatch_generator(const char __UNUSED__ *text, int __UNUSED__ gen_state) {
	return NULL;
}

/**
 * this function is called by readline to get each word that matches what is
 * being completed
 */
static char *url_match_generator(const char *text, int gen_state) {
	const char *name;
	char **list;
	unsigned tlen;

	// we should have a node that we are following
	if (!state.node)
		return NULL;

	// this is the first call for this completion
	if (!gen_state) {
		state.word_len = strlen(text);
		state.list_index = 0;
	}

	tlen = state.word_len;

	// a valid node has a static name or a way to generate a list
	while (state.node->name 
			|| state.node->gen_word_list) {

		// do we have a static name?
		name = state.node->name;
		if (name) {
			// advance to the next one regardless of outcome
			state.node++;
			state.list_index = 0;

			// if we match return it
			if (!strncmp (name, text, tlen)) {
				if (state.node->sub_nodes)
					rl_completion_append_character = '/';
				return strdup (name);
			}

			continue;
		}

		// if first time we look at this list, did it expire?
		if (!state.list_index && state.node->cached_list
				&& state.node->cache_expires < time(NULL)) {
			free_node_cached_words (state.node);
		}

		// do we need to create a cached list?
		list = state.node->cached_list;
		if (!list) {
			time_t expire;

			// get new list
			list = state.node->cached_list = state.node->gen_word_list (&expire);
			if (!list)
				return NULL;

			// on success, set the expire time
			state.node->cache_expires = expire;
		}

		// walk the list we have starting at the last index
		while ( (name = list[state.list_index]) ) {
			// advance to the next one regardless of outcome
			state.list_index ++;

			if (!strncmp (name, text, tlen)) {
				if (state.node->sub_nodes)
					rl_completion_append_character = '/';
				return strdup (name);
			}
		}

		// advance to the next node
		state.node++;
		state.list_index = 0;
	}

	// no more matches
	return NULL;
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

	user = state.url_words[state.url_word_count-1];

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




/* ------------------------------------------------------------------------ */

static char **get_lastfm_url_words(const char *url) {
	const char *c;
	char **url_words, *p;
	unsigned max_word_count, url_word_count;

	if (strncmp (url, "lastfm://", 9))
		return NULL;

	for (c=url, max_word_count=2; *c; c++)
		if (*c == '/') max_word_count++;

	url_words = calloc (max_word_count, sizeof(char*));

  /* clone the buffer, and split it into words on '/'
	 * start after the "lastfm://" prefix */
  url_words[0] = strdup (url+9);
	for (url_word_count=1, p=url_words[0]; *p; p++)
		if (*p=='/') {
			*p = 0;
			url_words[url_word_count++] = p+1;
		}

	return url_words;
}

static void free_lastfm_url_words(char **url_words) {
	free (url_words[0]);
	free (url_words);
}

static url_word_node_t* find_word_in_table(const char *word, url_word_node_t *table) {
	url_word_node_t *node;

	for (node = table; node->name || node->gen_word_list; node++) {

		if (node->name && !strcmp(word,node->name))
			return node->sub_nodes;

		if (node->gen_word_list)
			return node->sub_nodes;
	}

	return NULL;
}

static void free_node_cached_words (url_word_node_t *node) {
	int i;
	if (!node->cached_list)
		return;
	for (i=0; node->cached_list[i]; i++)
		free (node->cached_list[i]);
	free (node->cached_list);
	node->cached_list = NULL;
}

/* ------------------------------------------------------------------------ */

static int matches_one_of (const char *word, ...) {
	va_list ap;
	const char *s;
	int matched = 0;

	va_start(ap, word);

	while ((s = va_arg(ap, char *))) {
		if (!strcmp(word, s)) {
			matched = 1;
			break;
		}
	}

	va_end(ap);

	return matched;
}

