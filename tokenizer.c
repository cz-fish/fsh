#include <string.h>
#include <stdlib.h>

#include "tokenizer.h"

char* tok_names[] = {
	"[eof]",
	"[word]",
	"[assignment]",
	"[name]",
	"[newline]",
	"[i/o number]",
	
	"T__OPERATOR_START",
	"&&",
	"||",
	"&",
	"|",
	";",
	";;",
	"T__IO_OPEATOR_START",
	"<",
	"<<",
	">",
	">>",
	"<&",
	">&",
	"<>",
	"<<-",
	">|",
	"T__IO_OPERATOR_END",
/*	"T_EQUAL", */
	"T__OPERATOR_END",
	
	"T__KEYWORD_START",
	"if",
	"then",
	"else",
	"elif",
	"fi",
	"do",
	"done",
	"case",
	"esac",
	"while",
	"until",
	"for",
	"in",
	"function",
	"{",
	"}",
	"(",
	")",
	"!",
	"T__KEYWORD_END",
	
	"[string]",
	"[subshell]",
	"[arithmetic]"
};

/* a function that will delete the token */
void tok_deleter (void *token)
{
	struct token_s *t = (struct token_s*) token;
	if (t == NULL) return;
	if (t->str != NULL) {
		free (t->str);
	}
	free (t);
}

/* compares len first characters of str against the list
 * of keywords and returns token id of the keyword that
 * it matches.
 * returns T_WORD if it doesn't match any */
int tok_match_keyword (const char *str, int len, int *flags)
{
	int i;
	int f;
	
	if (len == 1) {
		if (*str == '!') return T_BANG;
		if (*str == '{') return T_LBRACE;
		if (*str == '}') return T_RBRACE;
/*		if (*str == '(') return T_LPAR;
		if (*str == ')') return T_RPAR; */
	} else if (len == 2) {
		if (!strncmp("if", str, 2)) return T_IF;
		if (!strncmp("fi", str, 2)) return T_FI;
		if (!strncmp("do", str, 2)) return T_DO;
		if (!strncmp("in", str, 2)) return T_IN;
	} else if (len == 3) {
		if (!strncmp("for", str, 3)) return T_FOR;
	} else if (len == 4) {
		if (!strncmp("then", str, 4)) return T_THEN;
		if (!strncmp("else", str, 4)) return T_ELSE;
		if (!strncmp("elif", str, 4)) return T_ELIF;
		if (!strncmp("done", str, 4)) return T_DONE;
		if (!strncmp("case", str, 4)) return T_CASE;
		if (!strncmp("esac", str, 4)) return T_ESAC;
	} else if (len == 5) {
		if (!strncmp("while", str, 5)) return T_WHILE;
		if (!strncmp("until", str, 5)) return T_UNTIL;
	} else if (len == 8) {
		if (!strncmp("function", str, 8)) return T_FUNCTION;
	}

	/* go through the word and try to determine some flags */
	f = TFL_IDENT | TFL_NUMERIC;
	for (i=0; i<len; i++) {
		if (str[i]<'0' || str[i]>'9') {
			/* not a numeric character, the word can't be numeric */
			f &= ~TFL_NUMERIC;
		}
		
		if ((str[i]>='0' && str[i]<='9') || (str[i]>='a' && str[i]<='z') || (str[i]>='A' && str[i]<='Z') || str[i] == '_') {
			if (i==0 && str[i]>='0' && str[i]<='9') {
				/* an identifier mustn't begin with a number */
				f &= ~TFL_IDENT;
			}
		} else {
			/* an identifier may only contain digits, letters and underscores */
			f &= ~TFL_IDENT;
		}
		
		if (str[i] == '\\') {i++; continue;}
		
		if (str[i] == '*' || str[i] == '?' || str[i] == '~' || str[i] == '[') {
			f |= TFL_EXPANDABLE;
		}
		if (str[i] == '=') {
			f |= TFL_ASSIGNMENT;
		}
	}
	
	(*flags) |= f;
	return T_WORD;
}

/* compares len first characters of str against the list
 * of operators and returns token id of the operator that
 * it matches.
 * returns T_WORD if it doesn't match any */
int tok_match_operator (const char *str, int len)
{
	if (len == 1) {
		if (*str == '&') return T_AND;
		if (*str == '|') return T_OR;
		if (*str == ';') return T_SEMI;
		if (*str == '>') return T_GREAT;
		if (*str == '<') return T_LESS;
	} else if (len == 2) {
		if (!strncmp("&&", str, 2)) return T_AND_IF;
		if (!strncmp("||", str, 2)) return T_OR_IF;
		if (!strncmp(";;", str, 2)) return T_DSEMI;
		if (!strncmp("<<", str, 2)) return T_DLESS;
		if (!strncmp(">>", str, 2)) return T_DGREAT;
		if (!strncmp("<&", str, 2)) return T_LESSAND;
		if (!strncmp(">&", str, 2)) return T_GREATAND;
		if (!strncmp("<>", str, 2)) return T_LESSGREAT;
		if (!strncmp(">|", str, 2)) return T_CLOBBER;
	} else if (len == 3) {
		if (!strncmp("<<-", str, 3)) return T_DLESSDASH;
	}
	return T_WORD;
}

int tok_next_token (const char *str, int *pos, int *start, int *end, struct token_s *token)
{
	int tok=T_EOF;
	int tmptok;
	char c;
	int i,j;
	token->complete = 1;
	token->tok = T_EOF;
	token->flags = TFL_NOFLAGS;
	*start = *pos;
	*end = *start;
	
	while (str[*pos] != 0) {
		if (	(str[*pos] != '\\') &&
			(is_operator(tok)) &&
			((tmptok = tok_match_operator(&str[*start], *pos-*start+1)) != T_WORD)
		   ) {
			/* apply tokenizer rule #2:
			 *   if the current char is unquoted
			 *     and the previous char was a part of an operator
			 *     and the current char appended to this operator forms another operator
			 *   then it's ok to append the current char to the operator */
			tok = tmptok;
			(*pos)++;
			continue;
		} else if (is_operator(tok)) {
			/* apply tokenizer rule #3:
			 *   if the current char cannot be appended to the last token
			 *     and the last token is an operator
			 *   then delimit the last token */
			break;
/*			*end = *pos - 1;
			return tok; */
		}

		/* apply tokenizer rules #4 and #5 (part):
		 *   if the current char is a backslash, a quote or a double-quote
		 *   then skip to the end of the escape sequence or the quotation resp. */
		if (str[*pos] == '\\') {
			if (tok == T_EOF) *start = *pos;
			if (str[*pos+1] != 0) {
				token->flags |= TFL_ESCAPED;
				(*pos)+=2;
			}
			else (*pos)++;
			tok=T_WORD;
			continue;
		} else if (str[*pos] == '\'' || str[*pos] == '"' || str[*pos] == '`') {
			if (tok == T_EOF) *start = *pos;
			/* set pos to the end of ' or " respectively */
			c = str[*pos];
			if (c=='`') token->flags |= TFL_SUBSHELL;
			else token->flags |= TFL_QUOTES;
			(*pos)++;
			i = 0;
			token->complete = 0;
			while (str[*pos]!=0) {
				if (i) {
					/* skip this character for it is escaped */
					i = 0;
					(*pos)++;
					continue;
				}
				if (str[*pos] == '\\') i = 1; /* the next character will be escaped */
				if (str[*pos] == c) {
					/* we've found the ending quotes */
					token->complete = 1;
					(*pos)++;
					break;
				}
				(*pos)++;
			}
/*			(*pos)--; */
			tok=T_WORD;
			continue;
		}
		
		/* apply tokenizer rule #5: 
		 *   if a $ is found and a { or a ( follows, read till the finishing } or ) occurs */
		if (str[*pos] == '$') {
			if (tok == T_EOF) *start = *pos;
			
			if (str[*pos+1] == '{') token->flags |= TFL_SUBSTITUTION;
			else if (str[*pos+1] == '(') {
				if (str[*pos+2] == '(') token->flags |= TFL_ARITHMETIC;
				else token->flags |= TFL_SUBSHELL;
			}
			token->flags |= TFL_SUBSTITUTION;
			
			if (str[*pos+1] == '{' || str[*pos+1] == '(') {
				i = 0;
				j = 1;  /* number of missing closing parentheses */
				c = str[*pos+1];
				*pos += 2;
				token->complete = 0;
				while (str[*pos] != 0) {
					if (i) {
						/* skip this character for it is escaped */
						i = 0;
						(*pos)++;
						continue;
					}
					if (str[*pos] == '\\') i = 1; /* the next character will be escaped */
					if (str[*pos] == c) j++;  /* we've found another opening brace or parenthesis */
					if ((str[*pos] == '}' && c=='{') || (str[*pos] == ')' && c=='(')) {
						/* we've found the enclosing brace or parenthesis */
						if (--j == 0) {
							token->complete = 1;
							(*pos)++;
							break;
						}
					}
					(*pos)++;
				}
				(*pos)--;
			} else if (str[*pos+1] == '#') {
				/* don't handle the # as a comment */
				(*pos)++;
			}
			(*pos)++;
			tok=T_WORD;
			continue;
		}
		
		/* apply tokenizer rule #6:
		 *   if the current char can start an operator, delimit the previous token */
		if ((tmptok = tok_match_operator(&str[*pos], 1)) != T_WORD) {
			if (tok != T_EOF) {
				/* delimit the previous token */
				break;
			}
			/* start a new token */
			tok = tmptok;
			*start = *pos;
			(*pos)++;
			continue;
		}

		/* apply tokenizer rule #7:
		 *   it the current char is a newline, delimit the previous token and make
		 *   newline the next token */
		if (str[*pos] == '\n') {
			if (tok != T_EOF) {
				break;
			}
			*start = *pos;
			tok=T_NEWLINE;
			(*pos)++;
			break;
		}
		
		/* apply tokenizer rule #8:
		 *   if the current char is a blank, delimit the previous token and discard
		 *     the blank */
		/* FIXME: dont strip \t when in here-document */
		if (str[*pos] == '\r' || str[*pos] == ' ' || str[*pos] == '\t') {
			if (tok != T_EOF) {
				break;
			}
			(*pos)++;
			continue;
		}
		
		/* apply tokenizer rule #10:
		 *   if the current character is a #, the rest of the line (excluding
		 *   first newline) will be discarded as a comment */
		if (str[*pos] == '#') {
			if (tok != T_EOF) {
				break;
			}
			
			(*pos)++;
			while (str[*pos] != 0) {
				if (str[*pos] == '\n') {
					break;
				}
				(*pos)++;
			}
			continue;
		}
		
		/* '(' and ')' will form separate tokens (T_LPAR) or
		 * (T_RPAR) respectively */
		if (str[*pos] == '(' || str[*pos] == ')') {
			if (tok != T_EOF) {
				break;
			}
			
			*start = *pos;
			if (str[*pos] == '(') tok = T_LPAR;
			else tok = T_RPAR;
			(*pos)++;
			break;
		}

		/* apply tokenizer rule #11:
		 *   if there's no token yet, make the current char the start of a new token */
		if (tok==T_EOF) {
			tok = T_WORD;
			*start = *pos;
		}

		/* apply tokenizer rule #9:
		 *   if the previous character was part of a word, the current character
		 *   should be appended to that word */
		(*pos)++;
	}
	/* we've reached the end of the string */
	/* apply tokenizer rule #1:
	 *   delimit last token and return it
	 *   if there was no previous token, return T_EOF */
	*end=*pos-1;
	token->tok = tok;
	token->str = (char*) &str[*start];
	token->len = *end - *start + 1;
	if (tok == T_WORD) {
		token->keyword = tok_match_keyword(&str[*start], *end-*start+1, &(token->flags));
		if (str[*pos] == ' ' || str[*pos] == '\r' || str[*pos] == '\t') {
			/* when a token is delimited by a blank character, it cannot form an IO_NUMBER token */
			token->flags &= ~TFL_NUMERIC;
		}
	} else {
		token->keyword = T_WORD;
	}
	return token->tok;
}

/* remove all ' " and \ left in the string, unless they are escaped or quoted */
int tok_quote_removal (char *buf)
{
	int i,j,eat=0;
	int inq=0,indq=0;

	if (buf == NULL) return 0;
	
	for (i=0, j=0; buf[j] != 0; j++) {
		if (eat) {
			eat = 0;
			buf[i++] = buf[j];
			continue;
		}
		if (buf[j] == '\\') {
			if (!inq && !indq) {
				/* unquoted \ will be skipped */
				eat = 1;
				continue;
			}
			if (inq) {
				if (buf[j+1] == '\'') {
					/* escaped single quote inside single quoted string */
					buf[i++] = '\'';
					j++;
					continue;
				}
				/* in other cases, copy the \ to the output */
			} else {
				if (buf[j+1] == '"') {
					/* escaped double quote inside double quoted string */
					buf[i++] = '"';
					j++;
					continue;
				}
				/* in other cases, copy the \ to the output */
			}
		}
		if (buf[j] == '"') {
			if (indq) {
				/* ending double quotes */
				indq = 0;
				continue;
			} else if (!inq) {
				/* opening double quotes */
				indq = 1;
				continue;
			}
			/* double quotes inside single quotes */
		}
		if (buf[j] == '\'') {
			if (inq) {
				/* ending single quotes */
				inq = 0;
				continue;
			} else if (!indq) {
				/* opening single quotes */
				inq = 1;
				continue;
			}
			/* single quotes inside double quotes */
		}
		buf[i++] = buf[j];
	}
	buf[i] = 0;
	/* if inq or indq is set here, there's an ending quotes missing in the string */
	if (inq || indq) return 1;
	return 0;
}

struct queue_s* tok_expand_and_enqueue (struct queue_s *queue, const struct token_s *token)
{
	struct queue_s *res;
	char buf[4096];
	struct token_s *tok;

	tok = (struct token_s*) malloc (sizeof (struct token_s));
	if (tok == NULL) {
		queue_clear (&queue);
		return NULL;
	}
	tok->tok = token->tok;
	tok->keyword = token->keyword;
	/* special case: when this token is an io operator and the previous
	 * token was numeric, the previous token will be rewritten to
	 * T_IO_NUMERIC */
	if (is_io_operator(tok->tok) && queue != NULL && (((struct token_s*)queue->data)->flags & TFL_NUMERIC)) {
		((struct token_s*)queue->data)->tok = T_IO_NUMBER;
	}
	
	if (tok->tok != T_WORD || tok->keyword != T_WORD) {
		/* we can skip all expansions if we have a keyword here */
		tok->str = (char *) malloc (strlen(buf)+1);
		if (tok->str == NULL) {
			queue_clear (&queue);
			return NULL;
		}
		strncpy (tok->str, token->str, token->len);
		tok->str[token->len] = 0;
		tok->flags = token->flags;
		res = queue_append (queue, (void*)tok, &tok_deleter);
		return res;
	}

	strncpy (buf, token->str, token->len);
	buf[token->len] = 0;
	
	/* these substitutions and expansions are done elsewhere */
	/* 2.5.1: positional parameters $1, $2, .. */
	/* 2.5.2: special parameters $# $? $@ .. */
	/* 2.6: word expansions */
	/* 2.6.1: tilde expansion */
	/* 2.6.2: parameter expansion */
	/* 2.6.3: command substitution */
	/* 2.6.4: arithmetic expansion */
	/* 2.6.5: field splitting */
	/* 2.6.6: pathname expansion */
	/* 2.6.7: quote removal */
/*	if (token->flags & (TFL_QUOTES | TFL_ESCAPED)) tok->complete = !tok_quote_removal (buf); */

	tok->str = (char *) malloc (strlen(buf)+1);
	if (tok->str == NULL) {
		queue_clear (&queue);
		return NULL;
	}
	strcpy (tok->str, buf);
	
	tok->flags = token->flags;
	res = queue_append (queue, (void*)tok, &tok_deleter);
	
	return res;
}

