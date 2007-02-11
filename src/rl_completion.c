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
#include "rl_completion.h"
#include "strarray.h"
#include "md5.h"

/* ------------------------------------------------------------------------ */
// keep track of the current completion state

cmpl_state_t cmpl_state;

// node utility functions
static cmpl_node_t* find_word_in_table(const char *word, cmpl_node_t *table);
static void free_node_cached_words (cmpl_node_t *node);

/* ------------------------------------------------------------------------ */

static char *_nomatch_generator(const char *text, int gen_state);
static char *_match_generator(const char *text, int gen_state);

char** cmpl_complete (const char *text, int __UNUSED__ start, int __UNUSED__ end, cmpl_node_t *table) {

	char **ret;
	char **words;
	int cnt;
	unsigned word_idx;
	char* (*match_generator)(const char*, int) = _nomatch_generator;

	strarray_init (&cmpl_state.url_sa);

	cnt = strarray_append_split (&cmpl_state.url_sa, rl_line_buffer, '/');
	if (cnt<=0)
		goto start_completion;

	// start at the first level
	word_idx = 0;
	words = cmpl_state.url_sa.strings;

	for (word_idx=0; (word_idx+1)<cmpl_state.url_sa.count; word_idx++) {
			table = find_word_in_table (words[word_idx], table);
			if (!table)
				break;
	}

	if (table) {
		// we found a place in the tree, use a real match generation function
		match_generator = _match_generator;

		// we have the table in which we are completing, we need to find the nodes
		// that make the most sense
		cmpl_state.node = table;
		cmpl_state.word_idx = word_idx;

		// we assume it's the last entry, this is updated in _match_generator()
		rl_completion_append_character = ' ';
	}

start_completion:
	// generate completions that match text
	rl_filename_completion_desired = 0;
	ret = rl_completion_matches(text, _match_generator);

	strarray_cleanup (&cmpl_state.url_sa);

	rl_filename_completion_desired = 0;
	rl_attempted_completion_over = 1;
	return ret;
}

/**
 * this function is called by readline to get each word that matches what is
 * being completed, except this is a decoy that generates no matches
 */
static char *_nomatch_generator(const char __UNUSED__ *text, int __UNUSED__ gen_state) {
	return NULL;
}

/**
 * this function is called by readline to get each word that matches what is
 * being completed
 */
static char *_match_generator(const char *text, int gen_state) {
	const char *name;
	char **list;
	unsigned tlen;

	// we should have a node that we are following
	if (!cmpl_state.node)
		return NULL;

	// this is the first call for this completion
	if (!gen_state) {
		cmpl_state.word_len = strlen(text);
		cmpl_state.list_index = 0;
	}

	tlen = cmpl_state.word_len;

	// a valid node has a static name or a way to generate a list
	while (cmpl_state.node->name 
			|| cmpl_state.node->gen_word_list) {

		// do we have a static name?
		name = cmpl_state.node->name;
		if (name) {
			// advance to the next one regardless of outcome
			cmpl_state.node++;
			cmpl_state.list_index = 0;

			// if we match return it
			if (!strncmp (name, text, tlen)) {
				if (cmpl_state.node->sub_nodes)
					rl_completion_append_character = '/';
				return strdup (name);
			}

			continue;
		}

		// if first time we look at this list, did it expire?
		if (!cmpl_state.list_index && cmpl_state.node->cached_list
				&& cmpl_state.node->cache_expires < time(NULL)) {
			free_node_cached_words (cmpl_state.node);
		}

		// do we need to create a cached list?
		list = cmpl_state.node->cached_list;
		if (!list) {
			time_t expire;

			// get new list
			list = cmpl_state.node->cached_list = cmpl_state.node->gen_word_list (&expire);
			if (!list)
				return NULL;

			// on success, set the expire time
			cmpl_state.node->cache_expires = expire;
		}

		// walk the list we have starting at the last index
		while ( (name = list[cmpl_state.list_index]) ) {
			// advance to the next one regardless of outcome
			cmpl_state.list_index ++;

			if (!strncmp (name, text, tlen)) {
				if (cmpl_state.node->sub_nodes)
					rl_completion_append_character = '/';
				return strdup (name);
			}
		}

		// advance to the next node
		cmpl_state.node++;
		cmpl_state.list_index = 0;
	}

	// no more matches
	return NULL;
}

/* ------------------------------------------------------------------------ */

static cmpl_node_t* find_word_in_table(const char *word, cmpl_node_t *table) {
	cmpl_node_t *node;

	for (node = table; node->name || node->gen_word_list; node++) {

		if (node->name && !strcmp(word,node->name))
			return node->sub_nodes;

		if (node->gen_word_list)
			return node->sub_nodes;
	}

	return NULL;
}

static void free_node_cached_words (cmpl_node_t *node) {
	int i;
	if (!node->cached_list)
		return;
	for (i=0; node->cached_list[i]; i++)
		free (node->cached_list[i]);
	free (node->cached_list);
	node->cached_list = NULL;
}

/* ------------------------------------------------------------------------ */

void save_rl_params(rl_params_t * rlp) {
	rlp->rl_basic_word_break_characters = rl_basic_word_break_characters; 
	rlp->rl_basic_word_break_characters = rl_basic_word_break_characters;
	rlp->rl_basic_word_break_characters = rl_basic_word_break_characters;
	rlp->rl_filename_completion_desired = rl_filename_completion_desired;
	rlp->rl_attempted_completion_over = rl_attempted_completion_over;
	rlp->rl_startup_hook = rl_startup_hook;
	rlp->rl_attempted_completion_function = rl_attempted_completion_function;
}

void restore_rl_params(const rl_params_t * rlp) {
	rl_basic_word_break_characters = rlp->rl_basic_word_break_characters; 
	rl_basic_word_break_characters = rlp->rl_basic_word_break_characters; 
	rl_basic_word_break_characters = rlp->rl_basic_word_break_characters;
	rl_filename_completion_desired = rlp->rl_filename_completion_desired;
	rl_attempted_completion_over = rlp->rl_attempted_completion_over;
	rl_startup_hook = rlp->rl_startup_hook;
	rl_attempted_completion_function = rlp->rl_attempted_completion_function;
}


