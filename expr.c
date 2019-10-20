#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "expr.h"
#include "support.h"

enum etok_e {
	ET_NUMB,        /* a constant number */
	ET_LPAR,        /* '(' */
	ET_RPAR,        /* ')' */
	ET_PLUS,        /* '+' */
	ET_MINUS,       /* '-' */
	ET_TIMES,       /* '*' */
	ET_DIV,         /* '/' */
	ET_MOD,         /* '%' */
	ET_AND,         /* '&' */
	ET_OR,          /* '|' */
	ET_XOR,         /* '^' */
	ET_NEG,         /* '~' */
	ET_POWER,       /* '**' */
	ET_LSL,         /* '<<' */
	ET_LSL_,        /* '<' invalid token */
	ET_LSR,         /* '>>' */
	ET_LSR_,        /* '>' invalid token */
	ET_NONE,
	ET_EOE          /* end of expression */
};

char* etok_names[] = {
	"number",       /* a constant number */
	"'('",            /* '(' */
	"')'",            /* ')' */
	"'+'",            /* '+' */
	"'-'",            /* '-' */
	"'*'",            /* '*' */
	"'/'",            /* '/' */
	"'%'",            /* '%' */
	"'&'",            /* '&' */
	"'|'",            /* '|' */
	"'^'",            /* '^' */
	"'~'",            /* '~' */
	"'**'",           /* '**' */
	"'<<'",           /* '<<' */
	"'<'",
	"'>>'",           /* '>>' */
	"'>'",
	"none",
	"end of expression"
};

struct etok_s {
	int tok;
	int val;
};

int err_occured;

#define flush_token \
	{ if (t == ET_LSL_ || t == ET_LSR_) {\
		error (ERR_EXPR, 0, "unknown symbol \"%s\"\n", etok_names[t]);\
		err_occured=1;\
	} else {\
		etok = (struct etok_s *) malloc (sizeof (struct etok_s *));\
		if (etok == NULL) {\
			error (ERR_EXPR, 0, "malloc error\n");\
			err_occured=1;\
			return ;\
		}\
		etok->tok = t;\
		etok->val = v;\
		*q = queue_append (*q, etok, &free);\
	}\
	t = ET_NONE;\
	v = 0; }


void etok_printer (int i, void *data)
{
	struct etok_s *e = (struct etok_s *) data;
	if (e->tok == ET_NUMB)
		error (ERR_DBG_PRINT_GENERAL, 1, "%02d number %d\n", i, e->val);
	else
		error (ERR_DBG_PRINT_GENERAL, 1, "%02d token %s\n", i, etok_names[e->tok]);
}

void lexan (char *str, struct queue_s **q)
{
	char *s = str;
	struct etok_s *etok;
	int t=ET_NONE;
	int v=0;
	int base = 10;
	int pc = 0;

	while (*s != 0) {
		if (*s >= '0' && *s <= '9') {
			if (t == ET_NUMB) {
				/* one more digit of the number */
				v = v * base + (*s) - '0';
				s++;
				continue;
			} else if (t != ET_NONE) {
				flush_token;
			}
			/* a new token */
			t = ET_NUMB;
			v = (*s) - '0';
		} else if (*s == '(') {
			if (t != ET_NONE) flush_token;
			t = ET_LPAR;
			flush_token;
			pc++;
		} else if (*s == ')') {
			if (t != ET_NONE) flush_token;
			t = ET_RPAR;
			flush_token;
			if (--pc < 0) {
				error (ERR_EXPR, 0, "too many right parentheses\n");
				err_occured = 1;
			}
		} else if (*s == '+') {
			if (t != ET_NONE) flush_token;
			t = ET_PLUS;
			flush_token;
		} else if (*s == '-') {
			if (t != ET_NONE) flush_token;
			t = ET_MINUS;
			flush_token;
		} else if (*s == '*') {
			if (t == ET_TIMES) {
				t = ET_POWER;
				flush_token;
				s++;
				continue;
			}
			if (t != ET_NONE) flush_token;
			t = ET_TIMES;
		} else if (*s == '/') {
			if (t != ET_NONE) flush_token;
			t = ET_DIV;
			flush_token;
		} else if (*s == '%') {
			if (t != ET_NONE) flush_token;
			t = ET_MOD;
			flush_token;
		} else if (*s == '&') {
			if (t != ET_NONE) flush_token;
			t = ET_AND;
			flush_token;
		} else if (*s == '|') {
			if (t != ET_NONE) flush_token;
			t = ET_OR;
			flush_token;
		} else if (*s == '^') {
			if (t != ET_NONE) flush_token;
			t = ET_XOR;
			flush_token;
		} else if (*s == '~') {
			if (t != ET_NONE) flush_token;
			t = ET_NEG;
			flush_token;
		} else if (*s == '<') {
			if (t == ET_LSL_) {
				t = ET_LSL;
				flush_token;
				s++;
				continue;
			}
			if (t != ET_NONE) flush_token;
			t = ET_LSL_;
		} else if (*s == '>') {
			if (t == ET_LSR_) {
				t = ET_LSR;
				flush_token;
				s++;
				continue;
			}
			if (t != ET_NONE) flush_token;
			t = ET_LSR_;
		} else if (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
			/* strip blank chars */
			if (t != ET_NONE) flush_token;
		} else {
			/* error */
			error (ERR_EXPR, 0, "unexpected character \"%c\"\n", *s);
			err_occured = 1;
		}
		s++;
	}
	if (t != ET_NONE) flush_token;
	t = ET_EOE;
	flush_token;
	if (pc > 0) {
		error (ERR_EXPR, 0, "missing %d left parenthes%cs\n", pc, pc>1?'e':'i');
		err_occured = 1;
	}
}

/* the grammar of an arithmetic expression:
 * 
 * Expr  -> A Expr_
 * Expr_ -> epsilon | '|' A Expr_
 * A     -> B A_
 * A_    -> epsilon | '^' A_
 * B     -> C B_
 * B_    -> epsilon | '&' B_
 * C     -> D C_
 * C_    -> epsilon | '<<' D C_ | '>>' D C_
 * D     -> F D_
 * D_    -> epsilon | '+' F D_ | '-' F D_
 * F     -> G F_
 * F_    -> epsilon | '*' G F_ | '/' G F_ | '%' G F_ | '**' G F_
 * G     -> H | '~' H | '+' H | '-' H
 * H     -> number | '(' Expr ')'
 * 
 */


int Expr  (struct queue_s *);
int Expr_ (struct queue_s *, int ival);
int A     (struct queue_s *);
int A_    (struct queue_s *, int ival);
int B     (struct queue_s *);
int B_    (struct queue_s *, int ival);
int C     (struct queue_s *);
int C_    (struct queue_s *, int ival);
int D     (struct queue_s *);
int D_    (struct queue_s *, int ival);
int F     (struct queue_s *);
int F_    (struct queue_s *, int ival);
int G     (struct queue_s *);
int H     (struct queue_s *);


int peek;
#define etok_peek {peek = ((struct etok_s *)queue_peek(q))->tok; }

#define etok_compare(x) \
{\
	if (peek == x) {\
		queue_throw_away(&q);\
		etok_peek;\
	} else {\
		error (ERR_EXPR, 0, "unexpected %s\n", etok_names[peek]);\
		err_occured=1;\
	}\
}

/* evaluate a constant expression, result is stored in res */
void eval_expr (char *str, int *res)
{
	struct queue_s *q = NULL;

	*res = 0;


/*	error (ERR_DBG_PRINT_GENERAL, 1, "arithmetic string: \"%s\"\n", str); */
	
	err_occured = 0;
	lexan (str, &q);
	if (err_occured) {
		queue_clear(&q);
		*res = 0;
		return;
	}

	/* DEBUG */
/*	error (ERR_DBG_PRINT_GENERAL, 1, "expr queue print:\n");
	queue_print (q, &etok_printer); */
	/* ENDDEBUG */
	
	if (q==NULL) return;

	etok_peek;
	*res = Expr (q);
	if (peek != ET_EOE) {
		if (peek == ET_NUMB) error (ERR_EXPR, 0, "unexpected number %d\n", ((struct etok_s*)queue_peek(q))->val);
		else error (ERR_EXPR, 0, "unexpected %s\n", etok_names[peek]);
		err_occured = 1;
	}
	
	if (err_occured) {
		*res = 0;
	}
	
	queue_clear (&q);
}


int Expr  (struct queue_s *q)
{
	return Expr_ (q, A(q));
}

int Expr_ (struct queue_s *q, int ival)
{
	int res;
	switch (peek) {
	case ET_OR:
		etok_compare (ET_OR);
		res = ival | A(q);
		return Expr_ (q,res);
	default:
		/* EMPTY */
		return ival;
	}
}

int A     (struct queue_s *q)
{
	return A_ (q, B(q));
}

int A_    (struct queue_s *q, int ival)
{
	int res;
	switch (peek) {
	case ET_XOR:
		etok_compare (ET_XOR);
		res = ival ^ B(q);
		return A_ (q,res);
	default:
		/* EMPTY */
		return ival;
	}
}

int B     (struct queue_s *q)
{
	return B_ (q, C(q));
}

int B_    (struct queue_s *q, int ival)
{
	int res;
	switch (peek) {
	case ET_AND:
		etok_compare (ET_AND);
		res = ival & C(q);
		return B_ (q,res);
	default:
		/* EMPTY */
		return ival;
	}
}

int C     (struct queue_s *q)
{
	return C_ (q, D(q));
}

int C_    (struct queue_s *q, int ival)
{
	int res;
	switch (peek) {
	case ET_LSL:
		etok_compare (ET_LSL);
		res = ival << D(q);
		return C_ (q,res);
	case ET_LSR:
		etok_compare (ET_LSR);
		res = ival >> D(q);
		return C_ (q,res);
	default:
		/* EMPTY */
		return ival;
	}
}

int D     (struct queue_s *q)
{
	return D_ (q, F(q));
}

int D_    (struct queue_s *q, int ival)
{
	int res;
	switch (peek) {
	case ET_PLUS:
		etok_compare (ET_PLUS);
		res = ival + F(q);
		return D_ (q,res);
	case ET_MINUS:
		etok_compare (ET_MINUS);
		res = ival - F(q);
		return D_ (q,res);
	default:
		/* EMPTY */
		return ival;
	}
}

int F     (struct queue_s *q)
{
	return F_ (q, G(q));
}

int F_    (struct queue_s *q, int ival)
{
	int res;
	switch (peek) {
	case ET_TIMES:
		etok_compare (ET_TIMES);
		res = ival * G(q);
		return F_ (q,res);
	case ET_DIV:
		etok_compare (ET_DIV);
		res = G(q);
		if (res == 0) {
			error (ERR_EXPR, 0, "division by zero\n");
			err_occured = 1;
		} else {
			res = ival / res;
		}
		return F_ (q,res);
	case ET_MOD:
		etok_compare (ET_MOD);
		res = G(q);
		if (res == 0) {
			error (ERR_EXPR, 0, "division by zero\n");
			err_occured = 1;
		} else {
			res = ival % res;
		}
		return F_ (q,res);
	case ET_POWER:
		etok_compare (ET_POWER);
		res = (int) pow (ival, G(q));
		return F_ (q,res);
	default:
		/* EMPTY */
		return ival;
	}
}

int G     (struct queue_s *q)
{
	switch (peek) {
	case ET_NEG:
		etok_compare (ET_NEG);
		return ~ H(q);
	case ET_PLUS:
		etok_compare (ET_PLUS);
		return H(q);
	case ET_MINUS:
		etok_compare (ET_MINUS);
		return - H(q);
	default:
		return H(q);	
	}
}

int H     (struct queue_s *q)
{
	int res;
	if (peek == ET_NUMB) {
		res = ((struct etok_s*)queue_peek(q))->val;
		etok_compare (ET_NUMB);
		return res;
	} else if (peek == ET_LPAR) {
		etok_compare (ET_LPAR);
		res = Expr (q);
		etok_compare (ET_RPAR);
		return res;
	} else {
		error (ERR_EXPR, 0, "unexpected %s\n", etok_names[peek]);
		err_occured = 1;
	}
	return 0;
}


