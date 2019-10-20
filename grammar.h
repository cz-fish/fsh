#ifndef __GRAMMAR_H__
#define __GRAMMAR_H__

#include "support.h"

enum gram_nodes_e {
	GRM_ROOT_NODE,        /* root node of the tree */
	GRM_LIST,             /* a list of 'and_or' nodes */
	GRM_ANDOR,            /* pipelines connected with && or || */
	GRM_BANG,             /* a ! */
	GRM_PIPELINE,         /* commands connected with | */
	GRM_COMMAND,          /* a simple command, a compound command or a function definition */
	GRM_SUBSHELL,
	GRM_COMPOUND_LIST,  /* a compound list of terms */
	GRM_NAME,
	GRM_WORD,
	GRM_FUNCTION,
	GRM_ASSIGNMENT_WORD,
	GRM_AND_IF,
	GRM_OR_IF,
	GRM_FOR,
	GRM_IN,
	GRM_CASE,
	GRM_CASEITEM,  /* one case item (one row in the case clause) */
	GRM_PATTERN,   /* a pattern of a case item */
/*	GRM_OR,      */  /* '|', logical or in case item pattern */
	GRM_IF,
	GRM_ELSE,
	GRM_ELIF,
	GRM_WHILE,
	GRM_UNTIL,
	GRM_SIMPLE_COM,  /* simple command */
	GRM_ASSIGNMENT,  /* an assignment in the form var=val */
	GRM_IO_REDIRECT,
	GRM_IO_NUMBER,
	GRM_LESS,
	GRM_DLESS,
	GRM_GREAT,
	GRM_DGREAT,
	GRM_LESSAND,
	GRM_GREATAND,
	GRM_LESSGREAT,
	GRM_DLESSDASH,
	GRM_CLOBBER,
	GRM_AND,

	GRM_NODES_END
};

extern char*gram_names[];

/* build the tree of abstract syntax reading the tokens from
 * a token queue, which was previously filled by tokenizer */
struct tree_s* gram_build_tree (struct queue_s *tok_q);

#endif
