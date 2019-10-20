#ifndef __TOKENIZER_H__
#define __TOKENIZER_H__

#include "support.h"

enum tok_tokens_e {
	T_EOF,
	T_WORD,
	T_ASSIGN,    /* ident in assignment */
	T_NAME,      /* ident */
	T_NEWLINE,
	T_IO_NUMBER,

	T__OPERATOR_START,
	T_AND_IF,
	T_OR_IF,
	T_AND,
	T_OR,
	T_SEMI,
	T_DSEMI,
	T__IO_OPERATOR_START,
	T_LESS,
	T_DLESS,
	T_GREAT,
	T_DGREAT,
	T_LESSAND,
	T_GREATAND,
	T_LESSGREAT,
	T_DLESSDASH,
	T_CLOBBER,
	T__IO_OPERATOR_END,
/*	T_EQUAL, */
	T__OPERATOR_END,

	T__KEYWORD_START,
	T_IF,
	T_THEN,
	T_ELSE,
	T_ELIF,
	T_FI,
	T_DO,
	T_DONE,
	T_CASE,
	T_ESAC,
	T_WHILE,
	T_UNTIL,
	T_FOR,
	T_IN,
	T_FUNCTION,
	T_LBRACE,
	T_RBRACE,
	T_LPAR,
	T_RPAR,
	T_BANG,
	T__KEYWORD_END,

	T_STRING,
	T_SUBSHELL,
	T_ARITHMETIC
};

#define is_operator(tok) ((tok) > T__OPERATOR_START && (tok) < T__OPERATOR_END)
#define is_io_operator(tok) ((tok) > T__IO_OPERATOR_START && (tok) < T__IO_OPERATOR_END)
#define is_keyword(tok) ((tok) > T__KEYWORD_START && (tok) < T__KEYWORD_END)

extern char* tok_names[];

#define TFL_NOFLAGS        0    /* no extra flags */
#define TFL_ASSIGNMENT     1    /* the word contains a = */
#define TFL_SUBSHELL       2    /* the token will require executing part of it in a subshell ('(' or '$(' or '`') */
#define TFL_ARITHMETIC     4    /* the token contains an arithmetic expression ('$((') */
#define TFL_QUOTES         8    /* the token contains quotes */
#define TFL_NUMERIC        16   /* the token contains only numbers */
#define TFL_IDENT          32   /* the token might be a name (an identifier) */
#define TFL_EXPANDABLE     64   /* the token will require a *,?,[],~ expansion */
#define TFL_ESCAPED        128  /* the token contains a backslash */
#define TFL_SUBSTITUTION   256  /* the token contains a $ substitution */

struct token_s {
	int tok;         /* type of the token (T_WORD for all keywords) */
	int keyword;     /* type of a keyword or T_WORD if the token is not a keyword */
	int complete;    /* 1 if the token in complete, 0 if it's not (like an enclosing parenthesis missing or something) */
	int flags;       /* combination of TFL_* flags specifying the token more precisely */
	char* str;       /* string of characters forming the token */
	int len;         /* length of the string */
};

/* a function that will delete the token */
void tok_deleter (void *token);

/* read and return next token from zero-terminated string str
 * the search for a token will start at position pos (it's wise to use the value of pos from
 *   the previous run of tok_next_token)
 * the start and end vars will be set to the index of beginning (or end resp.) of the token
 *   in str
 * the token parameter points to a token_s structure which will hold information about the
 *   next token */
int tok_next_token (const char *str, int *pos, int *start, int *end, struct token_s *token);

/* this function gets pointer to a queue of tokens or forms a new queue if queue is NULL,
 *   expands the given token according to token expansion rules and appends the expanded
 *   token (or more tokens if the expansion produced more than one token) to the end of the
 *   queue
 * returns a new pointer to the modified queue */
struct queue_s* tok_expand_and_enqueue (struct queue_s *queue, const struct token_s *token);

/* -- a set of functions used thruout the expansion -- */
/* remove all ' " and \ left in the string, unless they are escaped or quoted */
int tok_quote_removal (char *buf);


#endif
