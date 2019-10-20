#include <stdlib.h>
#include "tokenizer.h"
#include "grammar.h"
#include "pattern.h"

#define TRACE

char *gram_names[] = {
	"<root node>",
	"list",
	"and/or",
	"bang",
	"pipeline",
	"command",
	"subshell",
	"compound list",
	"name",
	"word",
	"function",
	"assignment word",
	"and if",
	"or if",
	"for",
	"in",
	"case",
	"case item",
	"pattern",
/*	"or", */
	"if",
	"else",
	"elif",
	"while",
	"until",
	"simple command",
	"assignment",
	"io redirect",
	"io number",
	"<",
	"<<",
	">",
	">>",
	"<&",
	">&",
	"<>",
	"<<-",
	">|",
	"&",
	"<end of nodelist>"
};	

int command_empty = 0;      /* may the command symbol expand to empty or not */
int complist_empty = 0;     /* may the compound_list symbol expand to empty or not */
int expand = 0;             /* tokens of type T_WORD shall be expanded */

int no_more_input = 0;      /* if set, the program won't ask for more input when T_EOF is encountered */
int pending = 0;
/* supportive functions working with the token queue */
struct token_s *toklast;
struct queue_s *q;

#define errtoken\
	error (ERR_SYNT, 0, "unexpected token \"%s\"\n", tok_names[toklast->tok]);

#define errmalloc\
	error (ERR_SYNT, 0, "internal malloc error");

#define add_node_nodata(t,x,y) \
	{x = tree_create_node (y, NULL, 0, NULL);\
	if (x == NULL) errmalloc;\
	if (tree_add_subnode (t, x)) errmalloc;}

#define add_node(t,x,y,z,u,f) \
	{x = tree_create_node (y, z, f, u);\
	if (x == NULL) errmalloc;\
	if (tree_add_subnode (t, x)) errmalloc;}

#define compare(x)\
	{if ((toklast->tok != x)) {\
		error (ERR_SYNT, 0, "unexpected token \"%s\", expecting \"%s\"\n", tok_names[toklast->tok], tok_names[x]);\
	} else if (toklast->tok == x) {\
		error (ERR_DBG_PRINT_GRAMMAR,1,"  eaten token %s\n", tok_names[toklast->tok]);\
		queue_throw_away(&q);\
		toklast = (struct token_s*) queue_peek(q);\
	}}

#define compare_keyword(x)\
/*	{while (toklast->tok != T_WORD) {\
		if (toklast->tok == T_EOF && !no_more_input) {\
			if ((no_more_input = more_input (&q))) {\
				error (ERR_SYNT, 0, "unexpected end of command\n");\
				break;\
			} else {\
				queue_throw_away (&q);  */ /* throw the T_EOF away *//*\
				toklast = (struct token_s*) queue_peek (q);\
				continue;\
			}\
		}\*/\
	{if (toklast->tok != T_WORD) {\
		error (ERR_SYNT, 0, "unexpected token \"%s\", expecting keyword \"%s\"\n", tok_names[toklast->tok], tok_names[x]);\
/*		break;\ */\
	}\
	if (toklast->tok == T_WORD && toklast->keyword == T_WORD) \
		error (ERR_SYNT, 0, "unexpected word \"%s\", expecting keyword \"%s\"\n", toklast->str, tok_names[x]);\
	else if (toklast->tok == T_WORD && toklast->keyword != x)\
		error (ERR_SYNT, 0, "unexpected keyword \"%s\", expecting keyword \"%s\"\n", tok_names[toklast->keyword], tok_names[x]);\
	else if (toklast->tok == T_WORD && toklast->keyword==x) {\
		error (ERR_DBG_PRINT_GRAMMAR,1,"  eaten keyword %s\n", tok_names[toklast->keyword]);\
		queue_throw_away(&q);\
		toklast = (struct token_s*) queue_peek(q);\
	}}

#define compare_add_str(tree,node,tid,grm)\
	{if (toklast->tok != tid) {\
		error (ERR_SYNT, 0, "unexpected token \"%s\", expecting \"%s\"\n", (toklast->tok==T_WORD)?toklast->str:tok_names[toklast->tok], tok_names[tid]);\
	} else if (toklast->tok == tid) {\
		/* don't throw the token away as we need the string it points to */\
		add_node (tree,node,grm, toklast->str, &free, toklast->flags);\
		error (ERR_DBG_PRINT_GRAMMAR, 1, "  added token with data: \"%s\"\n", toklast->str);\
		queue_pop_keep_data (&q);\
		free (toklast);\
		toklast = (struct token_s*) queue_peek(q);\
	}}

#define expand_add_word(tree,node,expq)\
	if (toklast->tok != T_WORD) {\
		error (ERR_SYNT, 0, "unexpected token \"%s\", expecting a word\n", tok_names[toklast->tok]);\
	} else if (toklast->tok == T_WORD) {\
		if (toklast->flags & TFL_EXPANDABLE) {\
			if (pattern_filename_gen (toklast->str, &expq) == 0) {\
				/* no matches, add the pattern as a single word */\
				/* don't throw the token away as we need the string it points to */\
				add_node (tree,node,GRM_WORD, toklast->str, &free, toklast->flags);\
				error (ERR_DBG_PRINT_GRAMMAR, 1, "  added token with data: \"%s\"\n", toklast->str);\
				queue_pop_keep_data (&q);\
				free (toklast);\
			} else {\
				while (expq!=NULL) {\
					add_node (tree, node, GRM_WORD, expq->first->data, &free, 0)\
					error (ERR_DBG_PRINT_GRAMMAR, 1, "  added token with data: \"%s\"\n", (char*)expq->first->data);\
					queue_pop_keep_data (&expq);\
				}\
				queue_throw_away (&q);\
			}\
		} else {\
			/* don't throw the token away as we need the string it points to */\
			add_node (tree,node,GRM_WORD, toklast->str, &free, toklast->flags);\
			error (ERR_DBG_PRINT_GRAMMAR, 1, "  added token with data: \"%s\"\n", toklast->str);\
			queue_pop_keep_data (&q);\
			free (toklast);\
		}\
		toklast = (struct token_s*) queue_peek(q);\
	}}


int token_peek()
{
	return toklast->tok;

	if (toklast->tok != T_EOF) return toklast->tok;
	
	while (toklast->tok == T_EOF && !no_more_input) {
		if ((no_more_input = more_input (&q))) {
			error (ERR_SYNT, 0, "unexpected end of command\n");
			break;
		} else {
			queue_throw_away (&q);   /* throw the T_EOF away */
			toklast = (struct token_s*) queue_peek (q);
			continue;
		}
	}

	return toklast->tok;
}

/* each of these functions handles one non-terminal */
void complete_command (struct tree_s *t);
void complete_command_rest (struct tree_s *t);
void list (struct tree_s *t);
void list_rest (struct tree_s *t);
void and_or (struct tree_s *t);
void and_or_rest (struct tree_s *t);
void pipeline (struct tree_s *t);
void pipe_sequence (struct tree_s *t);
void pipe_sequence_rest (struct tree_s *t);
void command (struct tree_s *t);
void command_rest (struct tree_s *t);
void compound_command (struct tree_s *t);
void subshell (struct tree_s *t);
void compound_list (struct tree_s *t);
void compound_list_rest (struct tree_s *t);
void term (struct tree_s *t);
void term_rest (struct tree_s *t);
void for_clause (struct tree_s *t);
void for_clause_rest_1 (struct tree_s *t);
void for_clause_rest_2 (struct tree_s *t);
void name (struct tree_s *t);
void in (struct tree_s *t);
void wordlist (struct tree_s *t);
void wordlist_rest (struct tree_s *t);
void case_clause (struct tree_s *t);
void case_list (struct tree_s *t);
void case_list_rest (struct tree_s *t);
void case_item_start (struct tree_s *t);
void case_item_body (struct tree_s *t);
void pattern (struct tree_s *t);
void pattern_rest (struct tree_s *t);
void if_clause (struct tree_s *t);
void if_clause_rest (struct tree_s *t);
void else_part (struct tree_s *t);
void while_clause (struct tree_s *t);
void until_clause (struct tree_s *t);
void function_definition (struct tree_s *t);
void function_body (struct tree_s *t);
void function_body_rest (struct tree_s *t);
void fname (struct tree_s *t);
void brace_group (struct tree_s *t);
void do_group (struct tree_s *t);
void simple_command (struct tree_s *t);
void simple_command_rest_1 (struct tree_s *t);
void simple_command_rest_2 (struct tree_s *t);
void cmd_name (struct tree_s *t);
void cmd_word (struct tree_s *t);
void cmd_prefix (struct tree_s *t);
void cmd_prefix_rest (struct tree_s *t);
void cmd_suffix (struct tree_s *t);
void cmd_suffix_rest (struct tree_s *t);
void redirection_list (struct tree_s *t);
void redirection_list_rest (struct tree_s *t);
void io_redirect (struct tree_s *t);
void io_redirect_rest (struct tree_s *t);
void io_file (struct tree_s *t);
void filename (struct tree_s *t);
void io_here (struct tree_s *t);
void here_end (struct tree_s *t);
void newline_list (struct tree_s *t);
void newline_list_rest (struct tree_s *t);
void linebreak (struct tree_s *t);
void separator_op (struct tree_s *t);
void separator (struct tree_s *t);
void sequential_sep (struct tree_s *t);

struct tree_s* gram_build_tree (struct queue_s *tok_q)
{
	struct tree_s *res = tree_create_node (GRM_ROOT_NODE, NULL, 0, NULL);
	error (ERR_NONE, 1, "");
	if (res == NULL) {
		error (ERR_SYNT, 0, "internal malloc error\n");
		return NULL;
	}
	no_more_input = 0;
	toklast = (struct token_s*) queue_peek (tok_q);
	q = tok_q;
	complete_command (res);
	return res;
}


void complete_command (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"complete_command\n");
#endif
	if (toklast->tok == T_NEWLINE || toklast->tok == T_EOF) return; /* an empty line */
	list (t);
	complete_command_rest (t);
}


void complete_command_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"complete_command_rest\n");
#endif
	switch (toklast->tok) {
	case T_AND:   /* '&' */
	case T_SEMI:  /* ';' */
	case T_NEWLINE:
		separator (t);
		break;
	default: /* EMPTY */
		;
	}
}


void list (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"list\n");
#endif
	add_node_nodata (t, a, GRM_LIST);
	and_or (a);
	list_rest (a);
}


void list_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"list_rest\n");
#endif
	switch (toklast->tok) {
	case T_AND:   /* '&' */
	case T_SEMI:  /* ';' */
		separator_op (t);
		command_empty = 1;
		and_or (t);
		command_empty = 0;
		list_rest (t);
		break;
	default: /* EMPTY */
		;
	}
}


void and_or (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"and_or\n");
#endif
	add_node_nodata (t, a, GRM_ANDOR);
	pipeline (a);
	and_or_rest (a);
}


void and_or_rest (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"and_or_rest\n");
#endif
	switch (toklast->tok) {
	case T_AND_IF:
		compare (T_AND_IF);
		add_node_nodata (t,a,GRM_AND_IF);
		linebreak (a);
		pipeline (a);
		and_or_rest (t);
		break;
	case T_OR_IF:
		compare (T_OR_IF);
		add_node_nodata (t,a,GRM_OR_IF);
		linebreak (a);
		pipeline (a);
		and_or_rest (t);
		break;
	default: /* EMPTY */
		;
	}
}


void pipeline (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"pipeline\n");
#endif
	if (toklast->tok == T_WORD && toklast->keyword == T_BANG) {
		compare_keyword (T_BANG);
		add_node_nodata (t, a, GRM_BANG);
		pipe_sequence (a);
	} else {
		/* might better be case of all elements in FIRST(pipe_sequence) */
		pipe_sequence (t);
	}
}


void pipe_sequence (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"pipe_sequence\n");
#endif
	add_node_nodata (t, a, GRM_PIPELINE);
	command (a);
	pipe_sequence_rest (a);
}


void pipe_sequence_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"pipe_sequence_rest\n");
#endif
	switch (toklast->tok) {
	case T_OR:  /* '|' */
		compare (T_OR);
		linebreak (t);
		command (t);
		pipe_sequence_rest (t);
		break;
	default: /* EMPTY */
		;
	}
}


void command (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"command\n");
	error (ERR_DBG_PRINT_TRACES,1,"%d, %d\n", toklast->tok, toklast->keyword);
#endif
	if (pending) token_peek();
	
	add_node_nodata (t, a, GRM_COMMAND);
	if ((toklast->tok == T_WORD &&
	    (toklast->keyword == T_THEN ||
	     toklast->keyword == T_ELIF ||
	     toklast->keyword == T_ELSE ||
	     toklast->keyword == T_FI ||
	     toklast->keyword == T_DO ||
	     toklast->keyword == T_RBRACE ||
	     toklast->keyword == T_ESAC ||
/*	     toklast->keyword == T_RPAR || */
	     toklast->keyword == T_DONE
	    )) ||
	    toklast->tok == T_RPAR) {
		/* EMPTY */
	} else if ((toklast->tok == T_WORD &&
	    (toklast->keyword == T_LBRACE ||
/*	     toklast->keyword == T_LPAR || */
	     toklast->keyword == T_FOR ||
	     toklast->keyword == T_CASE ||
	     toklast->keyword == T_IF ||
	     toklast->keyword == T_WHILE ||
	     toklast->keyword == T_UNTIL)) ||
	    toklast->tok == T_LPAR) {
		compound_command (a);
		command_rest (a);
	} else if (toklast->tok == T_WORD &&
	    toklast->keyword == T_FUNCTION) {
		function_definition (a);
	} else if (toklast->tok == T_LESS ||
	    toklast->tok == T_LESSAND ||
	    toklast->tok == T_GREAT ||
	    toklast->tok == T_GREATAND ||
	    toklast->tok == T_DGREAT ||
	    toklast->tok == T_LESSGREAT ||
	    toklast->tok == T_CLOBBER ||
	    toklast->tok == T_DLESS ||
	    toklast->tok == T_DLESSDASH ||
	    toklast->tok == T_IO_NUMBER ||
/*	    (toklast->tok == T_WORD && (toklast->flags & TFL_ASSIGNMENT)) || */
	    toklast->tok == T_WORD) {
		simple_command (a);
	} else if (toklast->tok == T_EOF || toklast->tok == T_NEWLINE) {
		/* in some cases, the command might be empty
		 * if it's an error, it must be handled elsewhere */
		/* EMPTY */
		if (!command_empty) errtoken;
	} else {
		errtoken;
	}
}

void command_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"command_rest\n");
#endif
	switch (toklast->tok) {
	case T_LESS:  /* '<' */
	case T_LESSAND:  /* '<&' */
	case T_GREAT:    /* '>' */
	case T_GREATAND:  /* '>&' */
	case T_DGREAT:  /* '>>' */
	case T_LESSGREAT:  /* '<>' */
	case T_CLOBBER:  /* '>|' */
	case T_DLESS:   /* '<<' */
	case T_DLESSDASH:  /* '<<-' */
	case T_IO_NUMBER:
		redirection_list (t);
		break;
	default: /* EMPTY */
		;
	}
}

void compound_command (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"compound_command\n");
#endif
	switch (toklast->keyword) {
	case T_LBRACE:
		brace_group (t);
		break;
/*	case T_LPAR:
		subshell (t);
		break; */
	case T_FOR:
		for_clause (t);
		break;
	case T_CASE:
		case_clause (t);
		break;
	case T_IF:
		if_clause (t);
		break;
	case T_WHILE:
		while_clause (t);
		break;
	case T_UNTIL:
		until_clause (t);
		break;
	default:
		if (token_peek() == T_LPAR) {
			subshell (t);
		} else {
			errtoken;
		}
	}
}


void subshell (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"subshell\n");
#endif
	compare (T_LPAR);
	add_node_nodata (t,a,GRM_SUBSHELL);
	compound_list (a);
	compare (T_RPAR);
}


void compound_list (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"compound_list\n");
#endif
	add_node_nodata (t, a, GRM_COMPOUND_LIST);
	linebreak (a);
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"  next token: %s, keyword %s\n", tok_names[toklast->tok], tok_names[toklast->keyword]);
#endif
	if ((toklast->tok == T_WORD &&
	    (toklast->keyword == T_THEN ||
	     toklast->keyword == T_ELIF ||
	     toklast->keyword == T_ELSE ||
	     toklast->keyword == T_FI ||
	     toklast->keyword == T_DO ||
	     toklast->keyword == T_RBRACE ||
	     toklast->keyword == T_ESAC ||
/*	     toklast->keyword == T_RPAR || */
	     toklast->keyword == T_DONE
	    )) ||
	    toklast->tok == T_RPAR) {
		/* EMPTY */
		if (!complist_empty) errtoken;
		/* FIXME: this allows an empty compound list */
	} else {  /* might better be case of all elements in FIRST(term->and_or->pipeline) */
		term (a);
		compound_list_rest (a);
	}
}


void compound_list_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"compound_list_rest\n");
#endif
	switch (toklast->tok) {
	case T_AND:   /* '&' */
	case T_SEMI:  /* ';' */
	case T_NEWLINE:
		separator (t);
		break;
	default: /* EMPTY */
		;
	}
}


void term (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"term\n");
#endif
	and_or (t);
	term_rest (t);
}


void term_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"term_rest\n");
#endif
	switch (toklast->tok) {
	case T_SEMI:
	case T_AND:
	case T_NEWLINE:
		separator (t);
		and_or (t);
		term_rest (t);
		break;
	default: /* EMPTY */
		;
	}
}


void for_clause (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"for_clause\n");
#endif
	compare_keyword (T_FOR);
	add_node_nodata (t,a,GRM_FOR);
	name (a);
	linebreak (a);
	for_clause_rest_1 (a);
}


void for_clause_rest_1 (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"for_clause_rest_1\n");
#endif
	switch (toklast->keyword) {
	case T_DO:
		do_group (t);
		break;
	case T_IN:
		in (t);
		for_clause_rest_2 (t);
		break;
	default:
		errtoken;
	}
}

void for_clause_rest_2 (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"for_clause_rest_2\n");
#endif
	switch (token_peek()) {
	case T_SEMI:
	case T_NEWLINE:
		sequential_sep (t);
		do_group (t);
		break;
	case T_WORD:
		expand = 1;
		wordlist (t);
		expand = 0;
		sequential_sep (t);
		do_group (t);
		break;
	default:
		errtoken;
	}
}


void name (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"name\n");
	error (ERR_DBG_PRINT_TRACES,1,"  name candidate: flags=%d\n", toklast->flags);
#endif
	/* grammar rule #5 */
	if (!(toklast->flags & TFL_IDENT)) error (ERR_SYNT, 0, "\"%s\" is not a valid name\n", toklast->str);
	compare_add_str (t, a, T_WORD, GRM_NAME);
}


void in (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"in\n");
#endif
	/* grammar rule #6 */
	compare_keyword (T_IN);
	add_node_nodata (t,a,GRM_IN);
}


void wordlist (struct tree_s *t)
{
	struct tree_s *a;
/*	struct queue_s *expq=NULL; */
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"wordlist\n");
#endif
	
	/* EXPAND */
/*	expand_add_word (t,a,expq); */
	compare_add_str (t,a,T_WORD,GRM_WORD);
	wordlist_rest (t);
}


void wordlist_rest (struct tree_s *t)
{
	struct tree_s *a;
/*	struct queue_s *expq=NULL; */
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"wordlist_rest\n");
#endif
	switch (toklast->tok) {
	case T_WORD:
		/* EXPAND */
/*		expand_add_word (t, a, expq); */
		compare_add_str (t,a,T_WORD,GRM_WORD);
		wordlist_rest (t);
		break;
	default: /* EMPTY */
		;
	}
}


void case_clause (struct tree_s *t)
{
	struct tree_s *a, *b;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"case_clause\n");
#endif
	compare_keyword (T_CASE);
	add_node_nodata (t,a,GRM_CASE);
	compare_add_str (a,b,T_WORD,GRM_WORD);
	linebreak (a);
	in (a);
	linebreak (a);
	case_list (a);
	compare_keyword (T_ESAC);
}


void case_list (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"case_list\n");
#endif
	if (toklast->tok == T_WORD && toklast->keyword != T_ESAC) {
		add_node_nodata (t,a, GRM_CASEITEM);
		case_item_start (a);
		linebreak (a);
		case_item_body (a);
		case_list_rest (t);
	}
	/* else EMPTY */
}


void case_list_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"case_list_rest\n");
#endif
	switch (toklast->tok) {
	case T_DSEMI:
		compare (T_DSEMI);
		linebreak (t);
		case_list (t);
		break;
	default:  /* NEWLINE or EMPTY */
		linebreak (t);
	}
}


void case_item_start (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"case_item_start\n");
#endif
	if (toklast->tok == T_LPAR) {
		/* LPAR keyword */
		compare (T_LPAR);
		pattern (t);
		compare (T_RPAR);
	} else if (toklast->tok == T_WORD && toklast->keyword != T_ESAC) {
		/* ordinary WORD */
		pattern (t);
		compare (T_RPAR);
	} else {
		errtoken;
	}
}


void case_item_body (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"case_item_body\n");
#endif
	linebreak (t);
	if (toklast->tok != T_DSEMI &&
	    (toklast->tok != T_WORD || toklast->keyword != T_ESAC)) {
		compound_list (t);
	}

	return;
	if ((toklast->tok == T_WORD &&
	     (/*   FIRST of pipeline */
	      toklast->keyword == T_BANG ||
	      /*   FIRST of command */
	      toklast->keyword == T_LBRACE ||
/*	      toklast->keyword == T_LPAR || */
	      toklast->keyword == T_FOR ||
	      toklast->keyword == T_CASE ||
	      toklast->keyword == T_IF ||
	      toklast->keyword == T_WHILE ||
	      toklast->keyword == T_UNTIL ||
	      /*     FIRST of function_definition */
	      toklast->keyword == T_FUNCTION ||
	      /*   FIRST of simple command */
	      (toklast->flags & TFL_ASSIGNMENT))) ||
	     toklast->tok == T_LESS ||
	     toklast->tok == T_LESSAND ||
	     toklast->tok == T_GREAT ||
	     toklast->tok == T_GREATAND ||
	     toklast->tok == T_DGREAT ||
	     toklast->tok == T_LESSGREAT ||
	     toklast->tok == T_CLOBBER ||
	     toklast->tok == T_DLESS ||
	     toklast->tok == T_DLESSDASH ||
	     toklast->tok == T_IO_NUMBER ||
	     toklast->tok == T_LPAR) {
		term (t);
		compound_list_rest (t);
	} else {
/*		linebreak (t); */
	}
}


void pattern (struct tree_s *t)
{
	struct tree_s *a, *b;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"pattern\n");
#endif
	/* grammar rule #4 */
	if (toklast->tok == T_WORD && toklast->keyword != T_ESAC && toklast->keyword != T_RPAR) {
		add_node_nodata (t, a, GRM_PATTERN);
		compare_add_str (a, b, T_WORD, GRM_WORD);
		pattern_rest (a);
	} else {
		errtoken;
	}
}


void pattern_rest (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"pattern_rest\n");
#endif
	if (toklast->tok == T_OR) {
		compare (T_OR);
/*		add_node_nodata (t, a, GRM_OR); */
		compare_add_str (t, a, T_WORD, GRM_WORD);
		pattern_rest (t);
	} /* else EMPTY */
}


void if_clause (struct tree_s *t)
{
	struct tree_s * a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"if_clause\n");
#endif
	compare_keyword (T_IF);
	add_node_nodata (t, a, GRM_IF);

	compound_list (a);
	compare_keyword (T_THEN);
	compound_list (a);
	pending = 1;
	if_clause_rest (a);
	pending = 0;
}


void if_clause_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"if_clause_rest\n");
#endif
	token_peek();
	switch (toklast->keyword) {
	case T_ELIF:
	case T_ELSE:
		else_part (t);
		compare_keyword (T_FI);
		break;
	case T_FI:
		compare_keyword (T_FI);
		break;
	default:
		errtoken;
	}
}


void else_part (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"else_part\n");
#endif
	switch (toklast->keyword) {
	case T_ELIF:
		compare_keyword (T_ELIF);
		add_node_nodata (t,a,GRM_ELIF);
		compound_list (a);
		compare_keyword (T_THEN);
		compound_list (a);
		else_part (t);
		break;
	case T_ELSE:
		compare_keyword (T_ELSE);
		add_node_nodata (t,a,GRM_ELSE);
		compound_list (a);
	default:
		;
	}
}


void while_clause (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"while_clause\n");
#endif
	compare_keyword (T_WHILE);
	add_node_nodata (t,a,GRM_WHILE);
	compound_list (a);
	do_group (a);
}


void until_clause (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"until_clause\n");
#endif
	compare_keyword (T_UNTIL);
	add_node_nodata (t,a, GRM_UNTIL);
	compound_list (a);
	do_group (a);
}


void function_definition (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"function_definition\n");
#endif
	compare_keyword (T_FUNCTION);
	add_node_nodata (t,a,GRM_FUNCTION);
	fname (a);
	compare (T_LPAR);
	compare (T_RPAR);
	linebreak (a);
	function_body (a);
}


void function_body (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"function_body\n");
#endif
	/* grammar rule #9 */
	compound_command (t);
	function_body_rest (t);
}


void function_body_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"function_body_rest\n");
#endif
	/* grammar rule #9 */
	switch (toklast->tok) {
	case T_LESS:  /* '<' */
	case T_LESSAND:  /* '<&' */
	case T_GREAT:    /* '>' */
	case T_GREATAND:  /* '>&' */
	case T_DGREAT:  /* '>>' */
	case T_LESSGREAT:  /* '<>' */
	case T_CLOBBER:  /* '>|' */
	case T_DLESS:   /* '<<' */
	case T_DLESSDASH:  /* '<<-' */
	case T_IO_NUMBER:
		redirection_list (t);
		break;
	default: /* EMPTY */
		;
	}
}


void fname (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"fname\n");
#endif
	/* grammar rule #8 */
	if (!(toklast->flags & TFL_IDENT)) error (ERR_SYNT, 0, "\"%s\" is not a valid function name\n", toklast->str);
	compare_add_str (t, a, T_WORD, GRM_NAME);
}


void brace_group (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"brace_group\n");
#endif
	compare_keyword (T_LBRACE);
	compound_list (t);
	compare_keyword (T_RBRACE);
}


void do_group (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"do_group\n");
#endif
	/* grammar rule #6 */
	compare_keyword (T_DO);
	compound_list (t);
	compare_keyword (T_DONE);
}


void simple_command (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"simple_command\n");
#endif
	add_node_nodata (t,a,GRM_SIMPLE_COM);
	expand = 1;

	if (toklast->tok == T_LESS ||
	    toklast->tok == T_LESSAND ||
	    toklast->tok == T_GREAT ||
	    toklast->tok == T_GREATAND ||
	    toklast->tok == T_DGREAT ||
	    toklast->tok == T_LESSGREAT ||
	    toklast->tok == T_CLOBBER ||
	    toklast->tok == T_DLESS ||
	    toklast->tok == T_DLESSDASH ||
	    toklast->tok == T_IO_NUMBER ||
	    (toklast->tok == T_WORD && toklast->flags & TFL_ASSIGNMENT)) {
		cmd_prefix (a);
		simple_command_rest_1 (a);
	} else if (toklast->tok == T_WORD) {
		cmd_name (a);
		simple_command_rest_2 (a);
	} else {
		errtoken;
	}

	expand = 0;
}


void simple_command_rest_1 (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"simple_command_rest_1\n");
#endif
	switch (toklast->tok) {
	case T_WORD:
		cmd_word (t);
		simple_command_rest_2 (t);
		break;
	default:  /* EMPTY */
		;
	}
}

void simple_command_rest_2 (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"simple_command_rest_2\n");
#endif
	switch (toklast->tok) {
	case T_LESS:  /* '<' */
	case T_LESSAND:  /* '<&' */
	case T_GREAT:    /* '>' */
	case T_GREATAND:  /* '>&' */
	case T_DGREAT:  /* '>>' */
	case T_LESSGREAT:  /* '<>' */
	case T_CLOBBER:  /* '>|' */
	case T_DLESS:   /* '<<' */
	case T_DLESSDASH:  /* '<<-' */
	case T_IO_NUMBER:
	case T_WORD:
		cmd_suffix (t);
		break;
	default:   /* EMPTY */
		;
	}
}


void cmd_name (struct tree_s *t)
{
	struct tree_s *a;
/*	struct queue_s *expq=NULL; */
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"cmd_name\n");
#endif
	/* grammar rule #7a */
	/* EXPAND */
/*	expand_add_word (t,a,expq); */
	compare_add_str (t,a,T_WORD,GRM_WORD);
}


void cmd_word (struct tree_s *t)
{
	struct tree_s *a;
/*	struct queue_s *expq=NULL; */
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"cmd_word\n");
#endif
	/* grammar rule #7b */
	/* EXPAND */
/*	expand_add_word (t,a,expq); */
	compare_add_str (t,a,T_WORD,GRM_WORD);
}


void cmd_prefix (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"cmd_prefix\n");
#endif
	if (toklast->tok == T_LESS ||
	    toklast->tok == T_LESSAND ||  /* '<&' */
	    toklast->tok == T_GREAT ||    /* '>' */
	    toklast->tok == T_GREATAND ||  /* '>&' */
	    toklast->tok == T_DGREAT ||  /* '>>' */
	    toklast->tok == T_LESSGREAT ||  /* '<>' */
	    toklast->tok == T_CLOBBER ||  /* '>|' */
	    toklast->tok == T_DLESS ||   /* '<<' */
	    toklast->tok == T_DLESSDASH ||  /* '<<-' */
	    toklast->tok == T_IO_NUMBER) {
		io_redirect (t);
		cmd_prefix_rest (t);
	} else if (toklast->tok == T_WORD && toklast->flags & TFL_ASSIGNMENT) {
		toklast->tok = T_ASSIGN;
		compare_add_str (t,a,T_ASSIGN,GRM_ASSIGNMENT);
		cmd_prefix_rest (t);
	} else {
		errtoken;
	}
}


void cmd_prefix_rest (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"cmd_prefix_rest\n");
#endif
	if (toklast->tok == T_LESS ||
	    toklast->tok == T_LESSAND ||  /* '<&' */
	    toklast->tok == T_GREAT ||    /* '>' */
	    toklast->tok == T_GREATAND ||  /* '>&' */
	    toklast->tok == T_DGREAT ||  /* '>>' */
	    toklast->tok == T_LESSGREAT ||  /* '<>' */
	    toklast->tok == T_CLOBBER ||  /* '>|' */
	    toklast->tok == T_DLESS ||   /* '<<' */
	    toklast->tok == T_DLESSDASH ||  /* '<<-' */
	    toklast->tok == T_IO_NUMBER) {
		io_redirect (t);
		cmd_prefix_rest (t);
	} else if (toklast->tok == T_WORD && toklast->flags & TFL_ASSIGNMENT) {
		toklast->tok = T_ASSIGN;
		compare_add_str (t,a,T_ASSIGN,GRM_ASSIGNMENT);
		cmd_prefix_rest (t);
	} /* else EMPTY */
}


void cmd_suffix (struct tree_s *t)
{
	struct tree_s *a;
/*	struct queue_s *expq=NULL; */
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"cmd_suffix\n");
#endif
	if (toklast->tok == T_LESS ||
	    toklast->tok == T_LESSAND ||  /* '<&' */
	    toklast->tok == T_GREAT ||    /* '>' */
	    toklast->tok == T_GREATAND ||  /* '>&' */
	    toklast->tok == T_DGREAT ||  /* '>>' */
	    toklast->tok == T_LESSGREAT ||  /* '<>' */
	    toklast->tok == T_CLOBBER ||  /* '>|' */
	    toklast->tok == T_DLESS ||   /* '<<' */
	    toklast->tok == T_DLESSDASH ||  /* '<<-' */
	    toklast->tok == T_IO_NUMBER) {
		io_redirect (t);
		cmd_suffix_rest (t);
	} else if (toklast->tok == T_WORD) {
		/* EXPAND */
/*		expand_add_word (t, a, expq); */
		compare_add_str (t,a,T_WORD,GRM_WORD);
		cmd_suffix_rest (t);
	} else {
		errtoken;
	}
}


void cmd_suffix_rest (struct tree_s *t)
{
	struct tree_s *a;
/*	struct queue_s *expq=NULL; */
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"cmd_suffix_rest\n");
#endif
	if (toklast->tok == T_LESS ||
	    toklast->tok == T_LESSAND ||  /* '<&' */
	    toklast->tok == T_GREAT ||    /* '>' */
	    toklast->tok == T_GREATAND ||  /* '>&' */
	    toklast->tok == T_DGREAT ||  /* '>>' */
	    toklast->tok == T_LESSGREAT ||  /* '<>' */
	    toklast->tok == T_CLOBBER ||  /* '>|' */
	    toklast->tok == T_DLESS ||   /* '<<' */
	    toklast->tok == T_DLESSDASH ||  /* '<<-' */
	    toklast->tok == T_IO_NUMBER) {
		io_redirect (t);
		cmd_suffix_rest (t);
	} else if (toklast->tok == T_WORD) {
		/* EXPAND */
/*		expand_add_word (t, a, expq); */
		compare_add_str (t,a,T_WORD,GRM_WORD);
		cmd_suffix_rest (t);
	} /* else EMPTY */
}


void redirection_list (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"redirection_list\n");
#endif
	io_redirect (t);
	redirection_list_rest (t);
}


void redirection_list_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"redirection_list_rest\n");
#endif
	switch (toklast->tok) {
	case T_LESS:  /* '<' */
	case T_LESSAND:  /* '<&' */
	case T_GREAT:    /* '>' */
	case T_GREATAND:  /* '>&' */
	case T_DGREAT:  /* '>>' */
	case T_LESSGREAT:  /* '<>' */
	case T_CLOBBER:  /* '>|' */
	case T_DLESS:   /* '<<' */
	case T_DLESSDASH:  /* '<<-' */
	case T_IO_NUMBER:
		io_redirect (t);
		redirection_list_rest (t);
		break;
	default:  /* EMPTY */
		;
	}
}


void io_redirect (struct tree_s *t)
{
	struct tree_s *a, *b;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"io_redirect\n");
#endif
	add_node_nodata (t,a,GRM_IO_REDIRECT);
	switch (token_peek()) {
	case T_LESS:  /* '<' */
	case T_LESSAND:  /* '<&' */
	case T_GREAT:    /* '>' */
	case T_GREATAND:  /* '>&' */
	case T_DGREAT:  /* '>>' */
	case T_LESSGREAT:  /* '<>' */
	case T_CLOBBER:  /* '>|' */
		io_file (a);
		break;
	case T_DLESS:   /* '<<' */
	case T_DLESSDASH:  /* '<<-' */
		io_here (a);
		break;
	case T_IO_NUMBER:
		compare_add_str (a,b,T_IO_NUMBER,GRM_IO_NUMBER);
		io_redirect_rest (a);
		break;
	default:
		errtoken;
	}
}


void io_redirect_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"io_redirect_rest\n");
#endif
	switch (token_peek()) {
	case T_LESS:  /* '<' */
	case T_LESSAND:  /* '<&' */
	case T_GREAT:    /* '>' */
	case T_GREATAND:  /* '>&' */
	case T_DGREAT:  /* '>>' */
	case T_LESSGREAT:  /* '<>' */
	case T_CLOBBER:  /* '>|' */
		io_file (t);
		break;
	case T_DLESS:   /* '<<' */
	case T_DLESSDASH:  /* '<<-' */
		io_here (t);
		break;
	default:
		errtoken;
	}
}


void io_file (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"io_file\n");
#endif
	switch (token_peek()) {
	case T_LESS:  /* '<' */
		compare (T_LESS);
		add_node_nodata (t,a,GRM_LESS);
		break;
	case T_LESSAND:  /* '<&' */
		compare (T_LESSAND);
		add_node_nodata (t,a,GRM_LESSAND);
		break;
	case T_GREAT:    /* '>' */
		compare (T_GREAT);
		add_node_nodata (t,a,GRM_GREAT);
		break;
	case T_GREATAND:  /* '>&' */
		compare (T_GREATAND);
		add_node_nodata (t,a,GRM_GREATAND);
		break;
	case T_DGREAT:  /* '>>' */
		compare (T_DGREAT);
		add_node_nodata (t,a,GRM_DGREAT);
		break;
	case T_LESSGREAT:  /* '<>' */
		compare (T_LESSAND);
		add_node_nodata (t,a,GRM_LESSGREAT);
		break;
	case T_CLOBBER:  /* '>|' */
		compare (T_CLOBBER);
		add_node_nodata (t,a,GRM_CLOBBER);
		break;
	default:
		errtoken;
	}
	filename (t);
}


void filename (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"filename\n");
#endif
	/* grammar rule #2 */
	compare_add_str (t,a,T_WORD,GRM_WORD);
}


void io_here (struct tree_s *t)
{
	struct  tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"io_here\n");
#endif
	switch (token_peek()) {
	case T_DLESSDASH:  /* '<<-' */
		compare (T_DLESSDASH);
		add_node_nodata (t,a,GRM_DLESSDASH);
		break;
	case T_DLESS:      /* '<<' */
		compare (T_DLESS);
		add_node_nodata (t,a,GRM_DLESS);
		break;
	default:
		errtoken;
	}
}


void here_end (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"here_end\n");
#endif
	/* grammar rule #3 */
	compare_add_str (t,a,T_WORD,GRM_WORD);
}


void newline_list (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"newline_list\n");
#endif
	compare (T_NEWLINE);
	newline_list_rest (t);
}


void newline_list_rest (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"newline_list_rest\n");
#endif
	switch (toklast->tok) {
	case T_NEWLINE:  /* NEWLINE */
		compare (T_NEWLINE);
		newline_list_rest (t);
		break;
	default:      /* EMPTY */
		;
	}
}


void linebreak (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"linebreak\n");
#endif
	switch (toklast->tok) {
	case T_NEWLINE:  /* NEWLINE */
		newline_list (t);
		break;
	default:     /* EMPTY */
		;
	}
}


void separator_op (struct tree_s *t)
{
	struct tree_s *a;
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"separator_op\n");
#endif
	switch (token_peek()) {
	case T_AND:  /* '&' */
		compare (T_AND);
		add_node_nodata (t, a, GRM_AND);
		break;
	case T_SEMI: /* ';' */
		compare (T_SEMI);
		break;
	default:
		errtoken;
	}
}


void separator (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"separator\n");
#endif
	switch (token_peek()) {
	case T_SEMI:  /* ';' */
	case T_AND:   /* '&' */
		separator_op (t);
		linebreak (t);
		break;
	case T_NEWLINE:  /* NEWLINE */
		newline_list (t);
		break;
	default:
		errtoken;
	}
}


void sequential_sep (struct tree_s *t)
{
#ifdef TRACE
	error (ERR_DBG_PRINT_TRACES,1,"sequential_sep\n");
#endif

	switch (token_peek()) {
	case T_SEMI:  /* ';' */
		compare (T_SEMI);
		linebreak (t);
		break;
	case T_NEWLINE: /* NEWLINE */
		newline_list (t);
		break;
	default:
		errtoken;
	}
}




