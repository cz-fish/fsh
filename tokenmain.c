#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "tokenizer.h"
#include "grammar.h"

void printtree (int level, int type, void *data)
{
	while (level-- > 0) {
		putc (' ', stdout);
		putc (' ', stdout);
	}
	printf ("node %d [%s] data: %s\n", type, gram_names[type], (data == NULL)?"":(char*)data);
}

int err_occured;

void error (int err_scope, int clear_last, char *fmt, ...)
{
	static int ignore = 0;
	va_list args;

	if (ignore && !clear_last) return;

	va_start (args, fmt);
	switch (err_scope) {
	case ERR_LEX:
		fprintf (stderr, "tokenizer error: ");
		break;
	case ERR_SYNT:
		fprintf (stderr, "syntax error: ");
		break;
	case ERR_EXEC:
		fprintf (stderr, "execution error: ");
	}
	vfprintf (stderr, fmt, args);
	va_end (args);
	if (err_scope != ERR_NONE) err_occured = 1;
	
	if (clear_last) ignore = 0;
	else ignore = 1;
}


int main (int argc, char**argv)
{
	struct token_s token;
	char *pars = "prom=$hodnota {prikaz1 && prikaz2}>out 2>&1 #blabla\nprikaz3 <<-mlamoj\n	blabla\nbbbb\n	mlamoj\n";
	int pos = 0;
	int start,end;
	char tokenvalue[256];
	char *str;

/*	char buf[1024];
	int complete;	*/

	struct queue_s *q = NULL;
/*	struct token_s *t; */

	struct tree_s *tree = NULL;
	
	/* test tok_quote_removal */
	/*
	strcpy (buf, "bla'a\\n\\\"\"\\'a'\\\"\"");
	printf ("%d: %s\n", 1, buf);
	complete = tok_quote_removal (buf);
	printf ("%d [!complete %d]: %s\n", 1, complete, buf);

	strcpy (buf, "\\aaa\\a\\'\\b'\\b'\\\"\\b\"\\b\\\"\"");
	printf ("%d: %s\n", 2, buf);
	complete = tok_quote_removal (buf);
	printf ("%d [!complete %d]: %s\n", 2, complete, buf);
	
	strcpy (buf, "\\\"\"blblbkfd");
	printf ("%d: %s\n", 3, buf);
	complete = tok_quote_removal (buf);
	printf ("%d [!complete %d]: %s\n", 3, complete, buf);
	
	return 0;
	*/
	
	/* test tok_next_token */
	if (argc >= 2) str = argv[1]; else str = pars;
	printf ("parsing string:\n'%s'\n", str);
	while (tok_next_token(str, &pos, &start, &end, &token) != T_EOF) {
		/* enqueue */
		q = tok_expand_and_enqueue (q, &token);
		memset (tokenvalue, 0, sizeof(tokenvalue));
		strncpy (tokenvalue, &str[start], (end-start>254)?255:end-start+1);
		printf ("token %d,%d (%s,%s) flags %d [%d,%d, complete %d]: '%s'\n", token.tok, token.keyword, tok_names[token.tok], tok_names[token.keyword], token.flags, start, end, token.complete, tokenvalue);
	}
	q = tok_expand_and_enqueue (q, &token);
	printf ("token %d (%s)\n", token.tok, tok_names[token.tok]);

	/* test the token queue */
	/*
	printf ("\n=============\ndequeue:\n");
	while (q != NULL) {
		q = queue_extract (q, (void**) &t);
		printf ("%s\n", tok_names[t->tok]);
	}
	*/
	
	/* build semantic tree */
	err_occured = 0;
	tree = gram_build_tree (q);
	if (err_occured) return 1;
	
	/* dump the tree */
	printf ("\n============\ntree was build as follows:\n");
	tree_print (tree, 0, &printtree);
	
	return 0;
}

