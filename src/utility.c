/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
	
	Shell.FM - sckif.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#include <stdio.h>

#include <readline/readline.h>
#include "utility.h"


/* ------------------------------------------------------------------------ */

void save_rl_params(struct rl_params * rlp) {
	rlp->rl_basic_word_break_characters = rl_basic_word_break_characters; 
	rlp->rl_basic_word_break_characters = rl_basic_word_break_characters;
	rlp->rl_basic_word_break_characters = rl_basic_word_break_characters;
	rlp->rl_filename_completion_desired = rl_filename_completion_desired;
	rlp->rl_attempted_completion_over = rl_attempted_completion_over;
	rlp->rl_startup_hook = rl_startup_hook;
	rlp->rl_attempted_completion_function = rl_attempted_completion_function;
}

/* ------------------------------------------------------------------------ */

void restore_rl_params(const struct rl_params * rlp) {
	rl_basic_word_break_characters = rlp->rl_basic_word_break_characters; 
	rl_basic_word_break_characters = rlp->rl_basic_word_break_characters; 
	rl_basic_word_break_characters = rlp->rl_basic_word_break_characters;
	rl_filename_completion_desired = rlp->rl_filename_completion_desired;
	rl_attempted_completion_over = rlp->rl_attempted_completion_over;
	rl_startup_hook = rlp->rl_startup_hook;
	rl_attempted_completion_function = rlp->rl_attempted_completion_function;
}
