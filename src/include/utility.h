/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_UTILITY
#define SHELLFM_UTILITY

struct rl_params {
	const char *rl_basic_word_break_characters;
	const char *rl_completer_word_break_characters;
	int rl_filename_completion_desired;
	int rl_attempted_completion_over;
	char rl_completion_append_character;
	int (*rl_startup_hook)(void);
	char** (*rl_attempted_completion_function)(const char *, int, int);
};

extern void save_rl_params(struct rl_params *);
extern void restore_rl_params(const struct rl_params *);

#endif
