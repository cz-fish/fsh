#ifndef __LEXAN_H
#define __LEXAN_H

#ifdef __cplusplus
extern "C" {
#endif

enum tokens_e {
	TOK_STRING,             /* an unknown string */
	TOK_SEMICOLON,          /* ; */
	TOK_AMPERSAND,          /* & */
	TOK_AND,                /* && */
	TOK_OR,                 /* || */
	TOK_PIPE,               /* | */
	TOK_IF,
	TOK_THEN,
	TOK_ELSE,
	TOK_FI,
	TOK_FOR,
	TOK_IN,
	TOK_DO,
	TOK_DONE,
	TOK_CASE,
	TOK_ESAC
};

enum tokflags_e {
	TFL_WILDCARDS=1,        /* has wildcards in it, should go through filename generation */
	TFL_QUOT=2,             /* is enquoted in '' */
	TFL_DBLQUOT=4,          /* is enquoted in "" */
	TFL_ENDOFFLAGS
};
	
struct lextoken_s {
	enum tokens_e token;    /* type of token */
	unsigned char *str;     /* character representation of the token */
	unsigned int flags;
};

int get_next_token (unsigned char *str, unsigned int start, struct lextoken_s *result);

	
#ifdef __cplusplus
} /* extern C */
#endif

#endif /* __LEXAN_H */

