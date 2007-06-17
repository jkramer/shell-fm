/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_COMPLETION
#define SHELLFM_COMPLETION

#include <readline/readline.h>
#include "strarray.h"

/* a tree composed of this structure will define the completion logic */
typedef struct cmpl_node_s {
	const char *name;
	char** (*gen_word_list)(time_t *new_expie_time);
	char** cached_list;
	time_t cache_expires;
	struct cmpl_node_s *sub_nodes;
} cmpl_node_t;
#define URL_WORD_NODE_END { .name = NULL, .gen_word_list = NULL, }
#define CACHE_WORDS_SEC 60

/* during completion, the cmpl_state can be accessed from the callbacks in the
 * completion table */
typedef struct cmpl_state_s { 
	strarray_t url_sa;
	unsigned word_idx;
	cmpl_node_t *node;
	unsigned word_len;
	unsigned list_index;
} cmpl_state_t;
extern cmpl_state_t cmpl_state;

/* generic completion given a table of possible nodes */
extern char** cmpl_complete (const char *text, int start, int end, cmpl_node_t *table);

/* ------------------------------------------------------------------------ */

typedef struct rl_params {
	const char *rl_basic_word_break_characters;
	const char *rl_completer_word_break_characters;
	int rl_filename_completion_desired;
	int rl_attempted_completion_over;
	char rl_completion_append_character;
	int (*rl_startup_hook)(void);
	char** (*rl_attempted_completion_function)(const char *, int, int);
} rl_params_t;

extern void save_rl_params(rl_params_t *);
extern void restore_rl_params(const rl_params_t *);

#endif
