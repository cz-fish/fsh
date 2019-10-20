/*
 * fsh - Bourne wannabe compatible shell
 *
 * Filip Simek <simekf1@fel.cvut.cz> 2006-2007
 * 
 * X36PJP semestral project, FEE CTU Prague
 * 
 */


#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifndef __USE_BSD
  #define __USE_BSD
  #ifndef __USE_SVID
    #define __USE_SVID
    #include <stdlib.h>
    #undef __USE_SVID
  #else
    #include <stdlib.h>
  #endif
  #undef __USE_BSD
#else
  #ifndef __USE_SVID
    #define __USE_SVID
    #include <stdlib.h>
    #undef __USE_SVID
  #else
    #include <stdlib.h>
  #endif
#endif

#include <readline/readline.h>
#include <readline/history.h>
#include <libgen.h>

#ifndef __USE_UNIX98
  #define __USE_UNIX98
  #include <unistd.h>
  #undef __USE_UNIX98
#else
  #include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#include <fcntl.h>

#include "const.h"
#include "tokenizer.h"
#include "grammar.h"
#include "pattern.h"
#include "expr.h"

#define SH_VERSION 0.9

/* string constants */
char red[] = {27, 91, 51, 49, 59, 52, 48, 109, 0};
char unred[] = {27, 91, 48, 109, 0};
char green[] = {27, 91, 51, 50, 59, 52, 48, 109, 0};
char ungreen[] = {27, 91, 48, 109, 0};
char esc[] = {27,0};

/* variables needed for PS? substitution */
char *shellname;                   /* name of the shell (argv[0]) */
unsigned int nr_jobs = 0;          /* number of running jobs */
unsigned int nr_histrycmd = 1;     /* number of the current command in history */
unsigned int nr_cmd = 1;           /* number of the current command */

extern char **environ;

/* special $ variables */
int shellpid;                      /* process id of the shell */
int last_res;                      /* result of the last command ($?) */
int numargs=0;                     /* $# */
char **args;                       /* $* */
/*char *args[] = {"-l", "7", "r", "gghr2", NULL}; */

/* error tracking variables */
int err_occured;
int err_count;

int dbg_print = /*DBG_PRINT_TOKENS | DBG_PRINT_TRACES |*/ /*DBG_PRINT_TREE*/ /*| DBG_PRINT_GRAMMAR*/ /*|*/ /*DBG_PRINT_EXEC*/ /*| DBG_PRINT_PATTERN | DBG_PRINT_GENERAL*/ 0;
int allow_exec = 1;

/* parameters for a subprogram */
int inpipe;
int outpipe;
int errpipe;
int background;

/* parse the string of format a=b and set variable a to value b */
void do_assignment (char *assign_word, int flags);
/* apply variable substitutions on given string with given TFL_ flags */
char* do_substitution (char *str, int flags);

/* execute tree of abstract syntax */
void execute (struct tree_s *tree);
/* run a command */
int run_command (struct tree_s *tree);



/* ------ shell variables ------ */
/* struct describing one shell variable */
struct var_s {
	char *name;
	char *str;
	int flags;
};

struct llist_s *var_q=NULL;        /* linked list of variables */

/* find a variable in the variable list */
struct var_s* varlist_find (char *name);
/* find and change the value of a given variable
 * if the variable is not found, a new variable will be created */
void varlist_modify_or_add (char *name, char *str, int flags);
/* unsets a variable, does nothing if the variable was not set */
void varlist_unset (char *name);

void signal_handler (int sig)
{
	printf ("caught SIGINT\n");
	signal (SIGINT, signal_handler);
/*	if (sig == SIGINT) { */
/*		error (ERR_DBG_PRINT_GENERAL, 0, "shell caught SIGINT\n");
		return; */
/*	} */
}

/* implementation of variable handling functions */
void var_deleter (void *data)
{
	struct var_s *v;
	if (data == NULL) return;
	v = (struct var_s *) data;
	if (v->name != NULL) free (v->name);
	if (v->str != NULL) free (v->str);
	free (v);
}

/* find a variable in the variable list */
struct var_s* varlist_find (char *name)
{
	struct llist_s *ll = var_q;
	struct var_s *v;

	if (name == NULL) return NULL;

	while (ll != NULL) {
		v = (struct var_s*) ll->data;
		if (v->name != NULL && !strcmp (v->name, name)) return v;
		ll = llist_nextitem (ll);
	}
	
	return NULL;
}

/* find and change the value of a given variable
 * if the variable is not found, a new variable will be created */
void varlist_modify_or_add (char *name, char *str, int flags)
{
	struct var_s *v;

	if (name == NULL || str == NULL) return;
	
	if ((v=varlist_find(name)) == NULL) {
		/* add new variable */
		v = (struct var_s*) malloc (sizeof (struct var_s));
		if (v == NULL) {
			error (ERR_EXEC, 0, "internal error: cannot set variable value due to malloc error\n");
			return;
		}
		v->name = (char*) malloc (strlen (name)+1);
		if (v->name == NULL) {
			error (ERR_EXEC, 0, "internal error: cannot set variable value due to malloc error\n");
			free (v);
			return;
		}
		strcpy (v->name, name);
		v->str = (char*) malloc (strlen (str)+1);
		if (v->str == NULL) {
			error (ERR_EXEC, 0, "internal error: cannot set variable value due to malloc error\n");
			free (v->name);
			free (v);
			return;
		}
		strcpy (v->str, str);
		v->flags = (flags==-1)?0:flags;
		var_q = llist_add (var_q, v, &var_deleter);
	} else {
		/* modify existing variable */
		free (v->str);
		v->str = NULL;
		v->str = (char*) malloc (strlen (str)+1);
		if (v->str == NULL) {
			error (ERR_EXEC, 0, "internal error: cannot set variable value due to malloc error\n");
			return;
		}
		strcpy (v->str, str);
		if (flags != -1)
			v->flags = flags;
	}
}

/* unsets a variable, does nothing if the variable was not set */
void varlist_unset (char *name)
{
	struct llist_s *ll = var_q;
	struct var_s *v;

	if (name == NULL) return;

	while (ll != NULL) {
		v = (struct var_s*) ll->data;
		if (v->name != NULL && !strcmp (v->name, name)) {
			llist_remove (&var_q,ll);
			return;
		}
		ll = llist_nextitem (ll);
	}
}


/* ------ jobs ------ */
struct job_s {
	pid_t pid;          /* pid if the job */
	int jid;            /* job id of the job */
	int rec;            /* how recent this job is - unimplemented yet */
	char *cmd;          /* job command */
};

struct llist_s *job_q = NULL;

void job_deleter (void *data)
{
	struct job_s *d = (struct job_s*) data;
	if (d->cmd != NULL) free (d->cmd);
	free (d);
}

/* add new job to the queue */
int job_add (pid_t pid, char*cmd)
{
	struct job_s *j;
	j = (struct job_s*)malloc (sizeof (struct job_s));
	if (j == NULL) {
		error (ERR_EXEC, 0, "malloc error\n");
		return 0;
	}
	j->cmd = (char*)malloc (strlen (cmd)+1);
	if (j->cmd == NULL) {
		free (j);
		error (ERR_EXEC, 0, "malloc error\n");
		return 0;
	}
	j->pid = pid;
	j->jid = llist_size (job_q) + 1;
	j->rec = 0;
	strcpy (j->cmd, cmd);
	job_q = llist_add (job_q, j, &job_deleter);
	return j->jid;
}

/* return struct job_s of the job with given pid */
struct job_s* job_get_by_pid (pid_t pid)
{
	struct llist_s *ll = job_q;
	struct job_s *j;
	int c = 0;
	
	if (ll == NULL) return NULL;
	ll = ll->first;
	while (ll!=NULL) {
		c++;
		j = (struct job_s*) ll->data;
		j->jid = c;
		if (j->pid == pid) return j;
		ll = ll->prev;
	}
	return NULL;
}

/* return struct job_s of the job with given jid */
struct job_s* job_get_by_jid (int jid)
{
	struct llist_s *ll = job_q;
	struct job_s *j;
	int c = 0;
	
	if (ll == NULL) return NULL;
	
	ll = ll->first;
	while (ll!=NULL) {
		c++;
		j = (struct job_s*) ll->data;
		j->jid = c;
		if (c == jid) return j;
		ll = ll->prev;
	}
	return NULL;
}

/* remove job from the queue */
void job_done (pid_t pid)
{
	struct llist_s *ll = job_q;
	struct job_s *j;
	
	if (ll == NULL) return;
	ll = ll->first;
	while (ll!=NULL) {
		j = (struct job_s*) ll->data;
		if (j->pid == pid) {
			llist_remove (&job_q, ll);
			return;
		}
		ll = ll->prev;
	}
	return;
}

/* ------ built-in commands ------ */
/* structure describing built-in commands */
struct builtin_s {
	char *cmd;                 /* built-in command alias */
	int (*run) (int argc, char **argv);      /* function implementing the command */
};
/* functions implementing built-in commands */
int builtin_cd (int, char**);
int builtin_export (int, char**);
int builtin_unexport (int, char**);
int builtin_exec (int, char**);
int builtin_alias (int, char**);
int builtin_unalias (int, char**);
int builtin_debug (int, char**);
int builtin_jobs (int, char**);
int builtin_exit (int, char**);

struct builtin_s b_in_c [] = {
	{"cd",     &builtin_cd},
	{"export", &builtin_export},
	{"unexport", &builtin_unexport},
	{"exec",   &builtin_exec},
	{"alias",  &builtin_alias},
	{"unalias", &builtin_unalias},
	{"debug",  &builtin_debug},
	{"jobs",   &builtin_jobs},
	{"exit",   &builtin_exit},
	{NULL, NULL}
};

/* implementation of built-in commands */
int builtin_cd (int argc, char **argv)
{
	char *path;
	struct var_s *var;
	if (argc == 1) {
		var = varlist_find ("HOME");
		if (var == NULL) {
			error (ERR_EXEC, 0, "HOME not set\n");
			return 1;
		} else {
			path = var->str;
		}
	} else {
		path = argv[1];
	}
	
	if (chdir (path) != 0) {
		error (ERR_EXEC, 0, "can't chdir to \"%s\"\n", path);
		return 1;
	}
	return 0;
}

int builtin_export (int argc, char **argv)
{
	char *out;
	if (argc < 2) {
		error (ERR_EXEC, 0, "export requires an argument in the form VAR=VAL\n");
		return 1;
	}
	out = (char*)malloc(strlen(argv[1])+1);
	if (out == NULL) {
		error (ERR_EXEC, 0, "malloc error\n");
		return 1;
	}
	strcpy (out, argv[1]);
	if (putenv (out) == 0) do_assignment (out,0);
	else {
		error (ERR_EXEC, 0, "export command unsuccssful\n");
	}
	return 0;
}

int builtin_unexport (int argc, char **argv)
{
	if (argc < 2) {
		error (ERR_EXEC, 0, "unexport requires a variable name as an argument\n");
		return 1;
	}
	unsetenv (argv[1]);
	varlist_unset (argv[1]);
	return 0;
}

int builtin_exec (int argc, char **argv)
{
	execvp (argv[1], &argv[1]);
	switch (errno) {
	case ENOEXEC:
		error (ERR_EXEC, 0, "\"%s\" is not in a recognised format\n", argv[1]);
		break;
	default:
		error (ERR_EXEC, 0, "\"%s\" can't be run\n", argv[1]);
	}
	exit(127);
	return 0;
}

int builtin_alias (int argc, char **argv)
{
	/* TODO: aliases */
	return 0;
}

int builtin_unalias (int argc, char **argv)
{
	/* TODO: unalias */
	return 0;
}

int builtin_debug (int argc, char **argv)
{
	/* debug - sets debug variables (dbg_print, allow_exec) */
	if (argc < 2) {
		error (ERR_EXEC, 0, "usage: debug (ae | de | <number>)\n\twhere ae allows execution\n\t      de denies execution\n\t      <number> is a new value of dbg_print\n");
		return 1;
	}
	if (!strcmp (argv[1], "ae")) {
		allow_exec = 1;
	} else if (!strcmp (argv[1], "de")) {
		allow_exec = 0;
	} else {
		dbg_print = atoi(argv[1]);
	}
	
	return 0;
}

int builtin_jobs (int argc, char **argv)
{
	/* jobs - print a list of jobs */
	struct llist_s *ll = job_q;
	struct job_s *j;
	int c = 0;
	
	if (ll == NULL) return 0;
	
	ll = ll->first;
	while (ll!=NULL) {
		c++;
		j = (struct job_s*) ll->data;
		j->jid = c;
		printf ("[%d]%c %d Running\t%s\n", j->jid, ' ', j->pid, j->cmd);
		ll = ll->prev;
	}
	
	return 0;
}

int builtin_exit (int argc, char **argv)
{
	if (argc > 1) {
		exit (atoi (argv[1]));
	} else {
		exit (0);
	}
	return 0;
}


/* ------ other functions ------- */

/* universal error printing function */
void error (int err_scope, int clear_last, char *fmt, ...)
{
	static int ignore = 0;
	va_list args;

	if (ignore && !clear_last) return;

	va_start (args, fmt);
	switch (err_scope) {
	case ERR_LEX:
		fprintf (stderr, "%stokenizer error:%s ", red, unred);
		break;
	case ERR_SYNT:
		fprintf (stderr, "%ssyntax error:%s ", red, unred);
		break;
	case ERR_EXEC:
		fprintf (stderr, "%sexecution error:%s ", red, unred);
		break;
	case ERR_EXPR:
		fprintf (stderr, "%serror in expression:%s ", red, unred);
		break;
	case ERR_PATTERN:
		fprintf (stderr, "%spattern matching error:%s ", red, unred);
		break;
	case ERR_DBG_PRINT_TOKENS:
		if (!(dbg_print & DBG_PRINT_TOKENS)) return;
		fprintf (stderr, "%stoken:%s ", green, ungreen);
		break;
	case ERR_DBG_PRINT_TRACES:
		if (!(dbg_print & DBG_PRINT_TRACES)) return;
		fprintf (stderr, "%strace:%s ", green, ungreen);
		break;
	case ERR_DBG_PRINT_TREE:
		if (!(dbg_print & DBG_PRINT_TREE)) return;
		fprintf (stderr, "%stree:%s ", green, ungreen);
		break;
	case ERR_DBG_PRINT_GRAMMAR:
		if (!(dbg_print & DBG_PRINT_GRAMMAR)) return;
		fprintf (stderr, "%sgrammar:%s ", green, ungreen);
		break;
	case ERR_DBG_PRINT_EXEC:
		if (!(dbg_print & DBG_PRINT_EXEC)) return;
		fprintf (stderr, "%sexec:%s ", green, ungreen);
		break;
	case ERR_DBG_PRINT_PATTERN:
		if (!(dbg_print & DBG_PRINT_PATTERN)) return;
		fprintf (stderr, "%spattern:%s ", green, ungreen);
		break;
	case ERR_DBG_PRINT_GENERAL:
		if (!(dbg_print & DBG_PRINT_GENERAL)) return;
		fprintf (stderr, "%sgeneral:%s ", green, ungreen);
		break;
	}
	vfprintf (stderr, fmt, args);
	va_end (args);
	if (err_scope <= ERR_PATTERN) {
		err_occured = 1;
		err_count++;
		ignore = 1;
	}
	
	if (err_scope == ERR_NONE && clear_last) ignore = 0;
}

/* DEBUG only */
void printtree (char *padding, int type, void *data)
{
	fputs (padding, stdout);
	printf ("node %d [%s] data: %s\n", type, gram_names[type], (data == NULL)?"":(char*)data);
}

void printqueue (int i, void *data)
{
	struct token_s *t = (struct token_s *) data;
	printf ("%02d %s,%s\n", i, tok_names[t->tok], tok_names[t->keyword]);
}

void printlist (int i, void *data)
{
	char *s = (char*) data;
	printf ("%02d %s\n", i, s);
}

void printvars (int i, void *data)
{
	struct var_s *v = (struct var_s*) data;
	printf ("%02d %s -> %s\n", i, v->name, v->str);
}

/* ENDDEBUG */

/* substitute escape sequences of PS1 */
void subst_ps (const char *in, char *out, int outsize)
{
	int pos = 0;
	int i = 0, j;
	char buf[512];
	char buf1[512];
	int len;
	int writebuf;
	uid_t uid;
	time_t tt;
	struct tm* t;
	char *c;
	

	out[outsize-1] = 0;
	
	while (in[i] != 0 && pos <= outsize-2) {
		if (in[i] == '\\') {
			if (in[i+1]>='0' && in[i+1]<='7' && in[i+2]>='0' && in[i+2]<='7' && in[i+3]>='0' && in[i+3]<='7') {
				/* char of given octal code */
				out[pos++] = (in[i+1]-'0')*64 + (in[i+2]-'0')*8 + (in[i+3]-'0');
				i+=4;
				continue;
			}
		
			writebuf = 0;	
			switch (in[i+1]) {
			case 0:
				return;
			case 'a':  /* bell */
				out[pos++] = 7;
				break;
			case 'd':  /* date */
				tt = time(NULL);
				t = localtime(&tt);
				strftime (buf, 512, "%a %b %d", t);
				writebuf = 1;
				break;
			case 'e':  /* esc */
				out[pos++] = 27;
				break;
			case 'h':  /* hostname */
				gethostname (buf, 512);
				j = 0;
				while (buf[j] != 0) {
					if (buf[j] == '.') { buf[j] = 0; break; }
					j++;
				}
				writebuf = 1;
				break;
			case 'H':  /* long hostname */
				gethostname (buf, 512);
				writebuf = 1;
				break;
			case 'j':  /* nr of jobs */
				sprintf (buf, "%u", nr_jobs);
				writebuf = 1;
				break;
			case 'l':  /* basename of terminal device */
				/* TODO */
				break;
			case 'n':  /* lf */
				out[pos++] = '\n';
				break;
			case 'r':  /* cr */
				out[pos++] = '\r';
				break;
			case 's':  /* name of shell */
				strncpy (buf1, shellname, 512);
				strncpy (buf, basename (buf1), 512);
				writebuf = 1;
				break;
			case 't':  /* 24hr full time */
				tt = time(NULL);
				t = localtime(&tt);
				sprintf (buf, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
				writebuf = 1;
				break;
			case 'T':  /* 12hr full time */
				tt = time(NULL);
				t = localtime(&tt);
				sprintf (buf, "%02d:%02d:%02d", (t->tm_hour==12)?12:(t->tm_hour%12), t->tm_min, t->tm_sec);
				writebuf = 1;
				break;
			case '@':  /* 12hr time (hr:min) */
				tt = time(NULL);
				t = localtime(&tt);
				sprintf (buf, "%02d:%02d %cM", (t->tm_hour==12)?12:(t->tm_hour%12), t->tm_min, (t->tm_hour>12)?'P':'A');
				writebuf = 1;
				break;
			case 'A':  /* 24hr time (hr:min) */
				tt = time(NULL);
				t = localtime(&tt);
				sprintf (buf, "%02d:%02d", t->tm_hour, t->tm_min);
				writebuf = 1;
				break;
			case 'u':  /* username */
				/* FIXME: doesn't work for a su-ed user */
				strncpy(buf,getenv("USER"),512);
				writebuf = 1;
				break;
			case 'v':  /* shell version short */
			case 'V':  /* shell version long */
				sprintf (buf, "%g", SH_VERSION);
				writebuf = 1;
				break;
			case 'w':  /* cwd */
				if (getcwd(buf, 512) != NULL) {
					c = getenv ("HOME");
					if (c != NULL) {
						strncpy (buf1, c, 512);
						j = strlen (buf1);
						if ((!strncmp (buf1, buf, j)) && (j==strlen(buf) || buf[j]=='/')) {
							/* ~ substitution */
							sprintf(buf, "~%s", &buf[j]);
						}
					}
					writebuf=1;
				}
				break;
			case 'W':  /* basename of cwd */
				if (getcwd(buf1, 512) != NULL) {
					c = getenv ("HOME");
					if (c != NULL) {
						strncpy (buf, c, 512);
						j = strlen (buf);
						if ((!strncmp (buf1, buf, j)) && (j==strlen(buf1) || buf1[j]=='/')) {
							/* ~ substitution */
							sprintf(buf1, "~%s", &buf1[j]);
						}
					}
					strncpy (buf,basename(buf1),512);
					writebuf = 1;
				}
				break;
			case '!':  /* history number */
				sprintf (buf, "%u", nr_histrycmd);
				writebuf = 1;
				break;
			case '#':  /* command number */
				sprintf (buf, "%u", nr_cmd);
				writebuf = 1;
				break;
			case '$':  /* # if UID == 0, else $ */
				uid = geteuid ();
				if (uid == 0) out[pos++] = '#'; else out[pos++] = '$';
				break;
			case '\\':
				out[pos++] = '\\';
				break;
			default:
				out[pos++] = '\\';
				i--;
			}
			if (writebuf) {
				len = strlen (buf);
				strncpy (&out[pos], buf, (len > outsize-pos-1)?outsize-pos-1:len);
				if ((pos+=len) > outsize-1) pos=outsize-1;
			}
			
			i++;
		} else {
			out[pos++] = in[i];
		}
		i++;
	}
	out[pos] = 0;
}

int main (int argc, char** argv)
{
	/* prompt variables */
	char *ps1;
	char *input;
	char ps[1024];
	int done = 0;

	struct var_s *var;

	/* tokenizer variables */
	struct token_s token;
	int pos = 0;
	int start,end;
	char tokenvalue[256];
	char tree_print_buf[256];

	/* grammar variables */
	struct queue_s *q = NULL;
	struct tree_s *tree = NULL;

	int fpar;

	/* if there's an argument, consider it an input file and run non-interactively */
	if (argc > 1) {
		fpar = open (argv[1], O_RDONLY);
		if (fpar < 0) {
			error (ERR_EXEC, 0, "unable to open script file: \"%s\"\n", argv[1]);
			return 127;
		}
		dup2 (fpar, STDIN_FILENO);
	}
	
	signal (SIGINT, signal_handler);

/*	printf ("argc: %d, argv[1]: %s\n", argc, argv[1]); */
	shellname = argv[0];
	shellpid = getpid();
	
	if (environ != NULL) {
		start = dbg_print;
		dbg_print &= ~DBG_PRINT_GENERAL;
		while (environ[pos] != NULL) {
			do_assignment (environ[pos],0);
			pos++;
		}
		dbg_print = start;
		pos=0;
	}
	/* DEBUG */
	if (dbg_print & DBG_PRINT_GENERAL) {
		puts ("environment var_q:");
		llist_print (var_q, &printvars);
	}
	/* ENDDEBUG */

	varlist_modify_or_add("PS1", "\\e[33;40m\\s:\\w:\\#\\$\\e[0m ", 0);
	
	while (!done) {
		if (isatty (STDIN_FILENO)) {
			/* input from the terminal */
			var = varlist_find ("PS1");
			if (var == NULL) ps1 = PS1_DEFAULT;
			else ps1 = var->str;
		
/*			var = varlist_find ("PS2");
			if (var == NULL) ps2 = PS2_DEFAULT;
			else ps2 = var->str; */
		
			subst_ps (ps1, ps, 1024);
			input = readline (ps);
		} else {
			/* input from a redirected file */
			input = (char*) malloc (1024);
			if (input == NULL) break;
			if (fgets (input, 1024, stdin) == NULL) {
				free (input);
				input = NULL;
			}
		}

	
		if (input == NULL /*|| input[0] == 0*/) {
			/* EOF */
			if (isatty (STDIN_FILENO)) fputs ("\n", stdout);
			break; /*continue;*/
		}

		if (input[0] == 0) {
			/* a blank line */
			free (input);
			nr_histrycmd++;
			nr_cmd++;
			run_command(NULL);
			continue;
		}

		/* lexical analysis */
		pos = 0;
		error (ERR_DBG_PRINT_TOKENS,1,"\n============\ntokens:\n");
		while (tok_next_token(input, &pos, &start, &end, &token) != T_EOF) {
			/* enqueue */
			q = tok_expand_and_enqueue (q, &token);
			/* for DEBUG only: */
			if (dbg_print & DBG_PRINT_TOKENS) {
				memset (tokenvalue, 0, sizeof(tokenvalue));
				strncpy (tokenvalue, &input[start], (end-start>254)?255:end-start+1);
				error (ERR_DBG_PRINT_TOKENS,1,"%d,%d (%s,%s) flags %d [%d,%d, complete %d]: '%s'\n", token.tok, token.keyword, tok_names[token.tok], tok_names[token.keyword], token.flags, start, end, token.complete, tokenvalue);
			}
			/* -- end of DEBUG */
		}
		q = tok_expand_and_enqueue (q, &token);
		error (ERR_DBG_PRINT_TOKENS,1,"%d (%s)\n", token.tok, tok_names[token.tok]);

		if (dbg_print & DBG_PRINT_TOKENS) {
			printf ("==============================\ntoken queue:\n");
			queue_print (q, &printqueue);
		}
		
		free (input);
		nr_histrycmd++;
		nr_cmd++;

		/* syntax analysis and creation of semantic tree */
		err_occured = 0;
		err_count = 0;
		error (ERR_DBG_PRINT_TRACES,1,"\n===========\ntraces:\n");
		tree = gram_build_tree (q);
		error (ERR_DBG_PRINT_TRACES,1,"%s[32;40merrors:%s[0m %d\n", esc, esc, err_count);
		
		if (err_occured) {
			queue_clear (&q);
			continue;
		}
	
		/* dump the tree (for DEBUG only) */
		error (ERR_DBG_PRINT_TREE,1,"\n============\ntree:\n");
		if (dbg_print & DBG_PRINT_TREE) {
			tree_print (tree, 0, tree_print_buf, 256, &printtree);
		}
		/* -- end of DEBUG */

		/* execute */
		inpipe = 0;
		outpipe = 1;
		errpipe = 2;
		background = 0;
		execute (tree);

		queue_clear (&q);
		tree_clear (&tree);
	}
	
	return 0;
}

int more_input (struct queue_s **q)
{
	struct var_s *var;
	char *input, ps[1024];
	int pos, start, end;
	struct token_s token;
	char tokenvalue[256];
	char *ps2;

again:	
	if (isatty (STDIN_FILENO)) {
		/* input from the terminal */
		var = varlist_find ("PS2");
		if (var == NULL) ps2 = PS2_DEFAULT;
		else ps2 = var->str;
		
		subst_ps (ps2, ps, 1024);
		input = readline (ps);
	} else {
		/* input from a redirected file */
		input = (char*) malloc (1024);
		if (input == NULL) return 1;
		if (fgets (input, 1024, stdin) == NULL) {
			free (input);
			input = NULL;
		}
	}

	if (input == NULL) {
		/* EOF */
		if (isatty (STDIN_FILENO)) fputs ("\n", stdout);
		return 1;
	}

	if (input[0] == 0) {
		/* a blank line */
		free (input);
		goto again;
	}

	/* lexical analysis */
	pos = 0;
	error (ERR_DBG_PRINT_TOKENS,1,"\n============\ntokens:\n");
	while (tok_next_token(input, &pos, &start, &end, &token) != T_EOF) {
		/* enqueue */
		*q = tok_expand_and_enqueue (*q, &token);
		/* for DEBUG only: */
		if (dbg_print & DBG_PRINT_TOKENS) {
			memset (tokenvalue, 0, sizeof(tokenvalue));
			strncpy (tokenvalue, &input[start], (end-start>254)?255:end-start+1);
			error (ERR_DBG_PRINT_TOKENS,1,"%d,%d (%s,%s) flags %d [%d,%d, complete %d]: '%s'\n", token.tok, token.keyword, tok_names[token.tok], tok_names[token.keyword], token.flags, start, end, token.complete, tokenvalue);
		}
		/* -- end of DEBUG */
	}
	*q = tok_expand_and_enqueue (*q, &token);
	error (ERR_DBG_PRINT_TOKENS,1,"%d (%s)\n", token.tok, tok_names[token.tok]);

	if (dbg_print & DBG_PRINT_TOKENS) {
		printf ("==============================\ntoken queue:\n");
		queue_print (*q, &printqueue);
	}
		
	free (input);
	return 0;
}

void execute (struct tree_s *tree)
{
/*	int res; = last_res; */
	struct tree_s *node,*t,*t2,*t3;
	struct queue_s *q=NULL;
	int b;
	char *r;
	pid_t p, endpid;
	int status;
	struct job_s *j;
	
	if (tree == NULL) {
		error (ERR_EXEC, 0, "nothing to execute\n");
		return;
	}

/*	printf ("[%s]\n", gram_names[tree->type]); */
	switch (tree->type) {
	case GRM_BANG:
		node = tree_get_first_subnode (tree);
		if (node != NULL) {
			execute (node);
			if (last_res == 0) last_res = 1; else last_res = 0;
		}
		break;
	case GRM_PIPELINE:
		/* execute all the piped commands in reversed order (from last to first)
		 * and link their file descriptors together */

		/* FIXME */
		node = tree_get_first_subnode (tree);
		while (node != NULL) {
			execute (node);
			node = tree_get_next_subnode (tree);
		}
	
		/* wait for the last command to finish */
		break;
	case GRM_COMMAND:
		/* execute all subnodes
		 * but take care of IO_REDIRECT subnodes */

		/* FIXME */
		node = tree_get_first_subnode (tree);
		while (node != NULL) {
			execute (node);
			node = tree_get_next_subnode (tree);
		}
	
		break;
	case GRM_SUBSHELL:
		/* fork a new instance and continue executing the subtree */
		/* then send the return value back to the parent shell */
			
		if ((p = fork()) == -1) {
			/* error */
			error (ERR_EXEC, 0, "internal error - unable to run a new command\n");
		} else if (p==0) {
			/* child */
			/* continue with execution of the subtree */
			node = tree_get_first_subnode (tree);
			while (node != NULL) {
				execute (node);
				node = tree_get_next_subnode (tree);
			}
			exit(last_res);
		} else {
			/* parent - wait for subshell to complete */
			endpid = -1;
			while (p != endpid) {
				if ((endpid = wait (&status)) == -1) {
					error (ERR_EXEC, 0, "internal error - unable to wait for process to complete\n");
				} else if (WIFEXITED (status)) {
					/* child terminated by an exit() call */
					last_res = WEXITSTATUS(status);
					if (p != endpid) {
						/* endpid is not the process we're waiting for, it must have been a job */
						j = job_get_by_pid (endpid);
						if (j != NULL) {
							printf ("[%d] Done\t\t%s\n", j->jid, j->cmd);
							job_done (endpid);
						}
					}
				} else if (WIFSIGNALED(status)){
					/* child terminated by a signal */
					last_res = 128+WTERMSIG(status);
					if (p != endpid) {
						/* endpid is not the process we're waiting for, it must have been a job */
						j = job_get_by_pid (endpid);
						if (j != NULL) {
							printf ("[%d] Terminated\t\t%s\n", j->jid, j->cmd);
							job_done (endpid);
						}
					}
				}
			}
		}

		break;
	case GRM_FUNCTION:
		/* declare a new function */
		/* TODO */
		break;
	case GRM_AND_IF:
		/* execute children only if last command was successful */
		if (last_res == 0) {
			node = tree_get_first_subnode (tree);
			while (node != NULL) {
				execute (node);
				node = tree_get_next_subnode (tree);
			}
		}
		break;
	case GRM_OR_IF:
		/* execute children only if last command was unsuccessful */
		if (last_res != 0) {
			node = tree_get_first_subnode (tree);
			while (node != NULL) {
				execute (node);
				node = tree_get_next_subnode (tree);
			}
		}
		break;
		
	case GRM_FOR:
		/* ---------- for clause ---------- */
		last_res = 0;
		node = tree_get_first_subnode (tree);   /* name -> node */
		t = tree_get_next_subnode (tree);
		if (node == NULL || t==NULL) {
			error (ERR_EXEC, 0, "unexpected NULL pointer\n");
			return;
		}
		if (t->type == GRM_IN) {
			/* the for clause is of type: for name in list; do ... */
			t2 = tree_get_next_subnode (tree);
			while (t!= NULL && t->type != GRM_COMPOUND_LIST) {
				t = tree_get_next_subnode (tree);
			}
			if (t==NULL) {
				/* this is and error */
				error (ERR_EXEC, 0, "broken execution tree\n");
				return;
			}
			tree_get_first_subnode (tree);
			tree_get_next_subnode (tree);
			t2 = tree_get_next_subnode (tree);
			/* now t2 points to first list item
			 * t points to the compound list to be executed */
			while (t2 != t) {
				/* SUBSTITUTE */
				r = do_substitution ((char*)t2->data, t2->flags);
				/* expand */
				if (/*(t2->flags & TFL_EXPANDABLE) &&*/ pattern_filename_gen (r, &q)) {
					/* iterate thru the result of expansion */
					while (q!=NULL) {
						/* QUOTE REMOVAL */
						if (tok_quote_removal ((char*)q->first->data)) {
							/* TODO: NEED MORE INPUT */
						}
						varlist_modify_or_add((char*)node->data, (char*)q->first->data, -1);
						queue_throw_away (&q);
						/* execute compound list t */
						execute (t);
					}
				} else {
					/* QUOTE REMOVAL */
					if (tok_quote_removal (r)) {
						/* TODO: NEED MORE INPUT */
					}
					/* assign new value to the control variable */
					varlist_modify_or_add ((char*)node->data, r, -1);
					/* execute compound list */
					execute (t);
				}
				if (r != NULL && r != (char*)t2->data) free (r);
				/* move to next list item */
				t2 = tree_get_next_subnode(tree);
			}
		} else {
			/* the for clause is of type: for name do ... */
			/* t is a compound list to be executed */
			t = tree_get_next_subnode (tree);
			for (b=0; b<numargs; b++) {
				/* assign new value to the control variable */
				varlist_modify_or_add ((char*)node->data, args[b], -1);
				/* execute compound list */
				execute (t);
			}
		}
		break;
		
	case GRM_CASE:
		/* ---------- case clause ---------- */
		last_res = 0;
		node = tree_get_first_subnode (tree);   /* name -> node */
		
		if (node == NULL) {
			error (ERR_EXEC, 0, "unexpected NULL pointer\n");
			return;
		}
		
		/* SUBSTITUTE */
		r = do_substitution ((char*)node->data, node->flags);
		
		t = tree_get_next_subnode (tree);
		if (t==NULL) {
			if (r!= NULL && r!=(char*)node->data) free (r);
			error (ERR_EXEC, 0, "unexpected NULL pointer\n");
			return;
		}
		if (t->type != GRM_IN) {
			/* this is and error */
			if (r!= NULL && r!=(char*)node->data) free (r);
			error (ERR_EXEC, 0, "broken execution tree\n");
			return;
		}
		/* for each case-item */
		t = tree_get_next_subnode (tree);
		while (t != NULL) {
			if (t->type != GRM_CASEITEM) {
				/* this is and error */
				if (r!= NULL && r!=(char*)node->data) free (r);
				error (ERR_EXEC, 0, "broken execution tree\n");
				return;
			}
			t2 = tree_get_first_subnode (t);
			if (t2 == NULL || t2->type != GRM_PATTERN) {
				/* this is and error */
				if (r!= NULL && r!=(char*)node->data) free (r);
				error (ERR_EXEC, 0, "broken execution tree\n");
				return;
			}
			b = 0;   /* not matched yet */
			/* for each pattern of the current case item */
			t3 = tree_get_first_subnode (t2);
			while (t3 != NULL) {
				if (pattern_match ((char*)t3->data, r)) {
					b = 1;
					break;
				}
				t3 = tree_get_next_subnode (t2);
			}

			if (b) {
				/* pattern of the current case-item was matched */
				t2 = tree_get_next_subnode (t);
				while (t2 != NULL) {
					execute (t2);
					t2 = tree_get_next_subnode (t);
				}
				break;
			}
			
			t = tree_get_next_subnode (tree);
		}
		
		if (r!= NULL && r!=(char*)node->data) free (r);
		
		break;
		
	case GRM_IF:
		/* -------------- if clause -------------- */
		last_res = 0;
		node = tree_get_first_subnode (tree);   /* condition -> node */
		t = tree_get_next_subnode (tree);       /* then-part -> t */
		if (node == NULL || t==NULL) {
			error (ERR_EXEC, 0, "unexpected NULL pointer\n");
			return;
		}
		execute (node);
		if (last_res == 0) {
			execute (t);
			break;
		}
		t2 = tree_get_next_subnode (tree);      /* elif-part or else-part -> t2 */
		while (t2 != NULL) {
			if (t2->type == GRM_ELIF) {
				/* elif-part */
				node = tree_get_first_subnode (t2);   /* condition -> node */
				t = tree_get_next_subnode (t2);       /* then-part -> t */
				if (node == NULL || t==NULL) {
					error (ERR_EXEC, 0, "unexpected NULL pointer\n");
					return;
				}
				execute (node);
				if (last_res == 0) {
					execute (t);
					break;
				}
			} else if (t2->type == GRM_ELSE) {
				/* else-part */
				t = tree_get_first_subnode (t2);       /* then-part -> t */
				if (t==NULL) {
					error (ERR_EXEC, 0, "unexpected NULL pointer\n");
					return;
				}
				execute (t);
				break;
			} else {
				/* this is and error */
				error (ERR_EXEC, 0, "broken execution tree\n");
				return;
			}
			t2 = tree_get_next_subnode (tree);      /* elif-part or else-part -> t2 */
		}
		break;
		
	case GRM_WHILE:
		/* -------------- while clause -------------- */
		last_res = 0;
		node = tree_get_first_subnode (tree);   /* condition -> node */
		t = tree_get_next_subnode (tree);       /* do-group -> t */
		if (node == NULL || t==NULL) {
			error (ERR_EXEC, 0, "unexpected NULL pointer\n");
			return;
		}
		execute (node);
		while (last_res == 0) {
			execute (t);
			execute (node);
		}
		
		break;
	case GRM_UNTIL:
		/* -------------- until clause -------------- */
		last_res = 0;
		node = tree_get_first_subnode (tree);   /* condition -> node */
		t = tree_get_next_subnode (tree);       /* do-group -> t */
		if (node == NULL || t==NULL) {
			error (ERR_EXEC, 0, "unexpected NULL pointer\n");
			return;
		}
		execute (node);
		while (last_res != 0) {
			execute (t);
			execute (node);
		}
		break;
		
	case GRM_SIMPLE_COM:
		last_res = run_command (tree);
		break;
	case GRM_IO_REDIRECT:
		/* TODO: */
		break;
		
	/* nodes which never should be executed */
/*	case GRM_ASSIGNMENT_WORD: */
	case GRM_ASSIGNMENT:
		/* take care of `` in the assignment (token flag subshell?) */
		do_assignment ((char*)tree->data, tree->flags);
		break;
	case GRM_CASEITEM:
	case GRM_ELSE:
	case GRM_ELIF:
	case GRM_NAME:
	case GRM_WORD:
	case GRM_IN:
	case GRM_PATTERN:
	case GRM_IO_NUMBER:
	case GRM_LESS:
	case GRM_DLESS:
	case GRM_GREAT:
	case GRM_DGREAT:
	case GRM_LESSAND:
	case GRM_GREATAND:
	case GRM_LESSGREAT:
	case GRM_DLESSDASH:
	case GRM_CLOBBER:
		/* some error */
		error (ERR_EXEC, 0, "broken execution tree, trying to execute node \"%s\"\n", gram_names[tree->type]);
		break;
		
	case GRM_LIST:
	case GRM_COMPOUND_LIST:
		/* execute all child nodes in normal order, but some of the children
		 * may run in background */
		node = tree_get_first_subnode (tree);
		while (node != NULL) {
			t = tree_get_next_subnode (tree);
			if (t!=NULL && t->type == GRM_AND) {
				/* execute this child in background */
				b = background;
				background = 1;
				execute (node);
				background = b;
			} else {
				execute (node);
			}
			node = t;
		}
		break;
	default:
		/* root_node, andor  */
		/* execute recursively all children */
		node = tree_get_first_subnode (tree);
		while (node != NULL) {
			execute (node);
			node = tree_get_next_subnode (tree);
		}
	}
	
	return;
}

#define arg_free(x) \
	{if (x!=NULL) {\
		for (i=0;x[i]!=NULL;i++) free (x[i]);\
		free(x);\
	}}

#define add_arg_word(x)\
	{if (sy_argc >= maxarg - 1) {\
		tmp = sy_argv;\
		sy_argv = (char**) malloc (2*maxarg*sizeof(char*));\
		if (sy_argv == NULL) {\
			arg_free(tmp);\
			error (ERR_EXEC, 0, "malloc error\n");\
			return 127;\
		}\
		for (i=0;i<sy_argc;i++) sy_argv[i] = tmp[i];\
		free(tmp);\
		maxarg*=2;\
	}\
	s = (char*)malloc (strlen((char*)x)+1);\
	if (s==NULL) {\
		arg_free(sy_argv);\
		error (ERR_EXEC, 0, "malloc error\n");\
		return 127;\
	}\
	strcpy (s, (char*)x);\
	s[strlen((char*)x)]=0;\
	sy_argv[sy_argc++]=s;\
	sy_argv[sy_argc] = NULL;\
	}

int run_command (struct tree_s *t)
{
	struct tree_s *t2;
	int sy_argc=0;
	char **sy_argv=NULL;
	char **tmp;
	char *s,*r;
	int i;
	int maxarg=8;
	struct queue_s *q=NULL;
	pid_t p,endpid;
	int status;
	struct job_s *j;

	sy_argv = (char**) malloc (maxarg * sizeof (char*));
	if (sy_argv == NULL) {
		error (ERR_EXEC, 0, "malloc error\n");
		return 127;
	}
	sy_argv[0] = NULL;
	
	/* look for any jobs that have finished */
	while ((endpid = waitpid (-1, &status, WNOHANG)) > 0) {
		if (WIFEXITED (status)) {
			/* child terminated by an exit() call */
			last_res = WEXITSTATUS(status);
			j = job_get_by_pid (endpid);
			if (j != NULL) {
				printf ("[%d] Done\t\t%s\n", j->jid, j->cmd);
				job_done (endpid);
			}
		} else if (WIFSIGNALED(status)){
			/* child terminated by a signal */
			last_res = 128+WTERMSIG(status);
			j = job_get_by_pid (endpid);
			if (j != NULL) {
				printf ("[%d] Terminated\t\t%s\n", j->jid, j->cmd);
				job_done (endpid);
			}
		}
	}
	
	/* ASSIGNMENT */
	t2 = tree_get_first_subnode (t);
	while (t2 != NULL) {
		if (t2->type == GRM_ASSIGNMENT) {
			/* assignment */
			do_assignment ((char*) t2->data, t2->flags);
		} else if (t2->type == GRM_IO_REDIRECT) {
			/* io redirection */
			/* TODO */
		} else if (t2->type == GRM_WORD) {
			/* SUBSTITUTE */
			r = do_substitution ((char*)t2->data, t2->flags);
			/* EXPAND */
/*			if (t2->flags & TFL_EXPANDABLE) { */
				if (pattern_filename_gen (r, &q) != 0) {
					while (q!= NULL) {
						/* QUOTE REMOVAL */
						if (tok_quote_removal ((char*)q->first->data)) {
							/* TODO: NEED MORE INPUT */
						}
						add_arg_word ((char*)q->first->data);
						queue_throw_away (&q);
					}
				} else {
					/* QUOTE REMOVAL */
					if (tok_quote_removal (r)) {
						/* TODO: NEED MORE INPUT */
					}
					add_arg_word (r);
				}
/*			} else {
				add_arg_word (r);
			} */
			if (r != NULL && r != (char*)t2->data) free (r);
		}
		t2 = tree_get_next_subnode (t);
	}

	if (sy_argc < 1) {
		/* nothing to run */
		return last_res;
	}
	
	error (ERR_DBG_PRINT_EXEC, 1, "running command \"%s\"\n", sy_argv[0]);
	for (i=1;sy_argv[i]!=NULL;i++) {
		error (ERR_DBG_PRINT_EXEC, 1, "argv[%d] \"%s\"\n", i, sy_argv[i]);
	}

	if (err_occured) {
		error (ERR_DBG_PRINT_EXEC, 1, "there were errors, nothing will be run\n");
		arg_free(sy_argv);
		return 127;
	}

	/* run the program here */
	/* built-in commands have a higher priority */
	for (i=0; b_in_c[i].cmd != NULL; i++) {
		if (!strcmp(b_in_c[i].cmd, sy_argv[0])) {
			last_res = b_in_c[i].run (sy_argc, sy_argv);
			goto end;
		}
	}
	
	if (!allow_exec) goto end;
	
	if ((p = fork()) == -1) {
		/* error */
		error (ERR_EXEC, 0, "internal error - unable to run a new command\n");
	} else if (p==0) {
		/* child */
		execvp (sy_argv[0], sy_argv);
		switch (errno) {
		case ENOEXEC:
			error (ERR_EXEC, 0, "\"%s\" is not in a recognised format\n", sy_argv[0]);
			break;
		default:
			error (ERR_EXEC, 0, "\"%s\" can't be run\n", sy_argv[0]);
		}
		exit(127);
	} else {
		/* parent */
		if (background) {
			/* the program was run in background, we won't wait for it to complete */
			/* add it to joblist */
			i = job_add (p, sy_argv[0]);
			printf ("[%d] %d\n", i, p);
		} else {
			/* a foreground program, wait fot it to complete */
			endpid = -1;
			while (p != endpid) {
				if ((endpid = wait (&status)) == -1) {
					error (ERR_EXEC, 0, "internal error - unable to wait for process to complete\n");
				} else if (WIFEXITED (status)) {
					/* child terminated by an exit() call */
					last_res = WEXITSTATUS(status);
					if (p != endpid) {
						/* endpid is not the process we're waiting for, it must have been a job */
						j = job_get_by_pid (endpid);
						if (j != NULL) {
							printf ("[%d] Done\t\t%s\n", j->jid, j->cmd);
							job_done (endpid);
						}
					}
				} else if (WIFSIGNALED(status)){
					/* child terminated by a signal */
					last_res = 128+WTERMSIG(status);
					if (p != endpid) {
						/* endpid is not the process we're waiting for, it must have been a job */
						j = job_get_by_pid (endpid);
						if (j != NULL) {
							printf ("[%d] Terminated\t\t%s\n", j->jid, j->cmd);
							job_done (endpid);
						}
					}
				}
			}
		}
	}

end:
	arg_free(sy_argv);

	return last_res;
}

/* parse the string of format a=b and set variable a to value b */
void do_assignment (char *str, int flags)
{
	char *eq = str;
	char *var, *val, *r;

	/* find the first '=' character */
	while (*eq != 0) {
		if (*eq == '=') break;
		eq++;
	}
	
	if (*eq == 0 || eq == str) {
		/* not an assignment word */
		error (ERR_EXEC, 0, "word \"%s\" marked as assignment bud doesn't assign anything\n", str);
		return ;
	}
	
	var = (char *) malloc (eq-str+1);
	if (var == NULL) {
		error (ERR_EXEC, 0, "malloc error\n");
		return;
	}
	strncpy (var, str, eq-str);
	var[eq-str] = 0;

	if (*(eq+1) == 0) {
		/* empty val, unset var */
		varlist_unset (var);
	} else {
		val = (char *) malloc (strlen(str)-(eq-str));
		if (val == NULL) {
			error (ERR_EXEC, 0, "malloc error\n");
			return;
		}
		strcpy (val, eq+1);

		/* SUBSTITUTE (do not expand) */
		r = do_substitution(val, flags);
	
		/* QUOTE REMOVAL */
		if (tok_quote_removal (r)) {
			/* missing " */
			/* TODO: NEED MORE INPUT */
		}
		
		varlist_modify_or_add(var, r, -1);
		
		if (r != NULL && r != val) free(r);
		free (val);
	}
	
	free (var);
	if (dbg_print & DBG_PRINT_GENERAL) {
		puts ("var_q:");
		llist_print (var_q, &printvars);
	}
}

/* apply variable substitutions on given string with given TFL_ flags */
char* do_substitution (char *str, int flags)
{
	char *res, *exp, *swap;
	char buf[64];
	int val;
	int i,j,k,l;
	struct var_s *var;
	
#define RESSIZE 32768
	if (str == NULL) return NULL;
/*	if (!(flags & (TFL_ARITHMETIC | TFL_SUBSTITUTION | TFL_SUBSHELL))) {
		return str;
	} */

	res = (char *) malloc (RESSIZE);
	if (res == NULL) {
		error (ERR_EXEC, 0, "malloc error\n");
		return NULL;
	}

/*	if ((flags & TFL_SUBSTITUTION)) { */
	for (i=0,j=0; str[i] != 0; i++) {
		if (str[i] == '\\') {
			/* an escaped character */
			res[j++]=str[i];
			if (str[i+1] != 0) res[j++]=str[++i];
		} else if (str[i] == '\'') {
			/* string enquoted in single quotes */
			res[j++]=str[i];
			while (str[++i] != 0) {
				res[j++]=str[i];
				if (str[i] == '\'') break;
				if (str[i] == '\\') {
					/* an escaped character */
					if (str[i+1] != 0) res[j++]=str[++i];
				}
			}
		} else if (str[i] == '$') {
			if ((str[i+1] >='a' && str[i+1] <='z') || (str[i+1] >='A' && str[i+1] <='Z') || str[i+1]=='_') {
				/* variable substitution, the variable name is delimited by a character which is invalid for an identifier */
				for (k=i+2; str[k]!=0; k++) {
					if ((str[k]<'a' || str[k]>'z') && (str[k]<'A' || str[k]>'Z') && (str[k]<'0' || str[k]>'9') && str[k]!='_') break;
				}
				/* copy the variable name */
				exp = (char*) malloc (k-i);
				if (exp == NULL) {
					free (res);
					error (ERR_EXEC, 0, "malloc error\n");
					return NULL;
				}
				strncpy (exp, &str[i+1], k-i-1);
				exp[k-i-1] = 0;
				/* substitute variable */
				var = varlist_find (exp);
				free(exp);
				if (var != NULL) {
					l = strlen(var->str);
					l = l>(RESSIZE-i-1)?RESSIZE-i-1:l;
					strncpy (&res[j], var->str, l);
					j+=l;
				}
				i=k-1;
			} else if (str[i+1] == '#') {
				/* $# number of arguments */
				sprintf (buf, "%d", numargs);
				k = strlen(buf);
				k = k>(RESSIZE-i-1)?RESSIZE-i-1:k;
				strncpy (&res[j], buf, k);
				j+=k;
				i++;
			} else if (str[i+1] == '?') {
				/* $? last result */
				sprintf (buf, "%d", last_res);
				k = strlen(buf);
				k = k>(RESSIZE-i-1)?RESSIZE-i-1:k;
				strncpy (&res[j], buf, k);
				j+=k;
				i++;
			} else if (str[i+1] == '@') {
				/* $@ all arguments, in double quotes expanded as multiple tokens */
				/* not implemented */
				i++;
			} else if (str[i+1] == '*') {
				/* $* all arguments, in double quotes expanded as one token */
				for (l=0;l<numargs;l++) {
					k = strlen(args[l]);
					k = k>(RESSIZE-i-2)?RESSIZE-i-2:k;
					strncpy (&res[j], args[l], k);
					j+=k;
					if (l< numargs-1) {
						res[j++]=' ';
						res[j]=0;
					}
				}
				i++;
			} else if (str[i+1] == '-') {
				/* $- a string of all options currently set */
				/* not implemented */
				i++;
			} else if (str[i+1] == '!') {
				/* $! PID of the most recently launched background job */
				/* not implemented */
				i++;
			} else if (str[i+1] == '$') {
				/* $$ shell pid */
				sprintf (buf, "%d", shellpid);
				k = strlen(buf);
				k = k>(RESSIZE-i-1)?RESSIZE-i-1:k;
				strncpy (&res[j], buf, k);
				j+=k;
				i++;
			} else if (str[i+1] == '0') {
				/* $0 shell name */
				k = strlen(shellname);
				k = k>(RESSIZE-i-1)?RESSIZE-i-1:k;
				strncpy (&res[j], shellname, k);
				j+=k;
				i++;
			} else if (str[i+1] >= '1' && str[i+1] <= '9') {
				/* $0-$9 single argument */
				if (numargs < str[i+1]-'0') {
					i++;
				} else {
					k = strlen(args[str[i+1]-'1']);
					k = k>(RESSIZE-i-1)?RESSIZE-i-1:k;
					strncpy (&res[j], args[str[i+1]-'1'], k);
					j+=k;
					i++;
				}
			} else {
				/* output the '$' char */
				res[j++] = str[i];
			}
		} else {
			res[j++] = str[i];
		}
	}
	res[j] = 0;
	swap = (char*) malloc (strlen(res)+1);
	if (swap == NULL) {
		free (res);
		error (ERR_EXEC, 0, "malloc error\n");
		return NULL;
	}
	strcpy (swap, res);

	for (i=0,j=0; swap[i] != 0; i++) {
		if (swap[i] == '\\') {
			/* an escaped character */
			res[j++]=swap[i];
			if (swap[i+1] != 0) res[j++]=swap[++i];
		} else if (swap[i] == '\'') {
			/* string enquoted in single quotes */
			res[j++]=swap[i];
			while (swap[++i] != 0) {
				res[j++]=swap[i];
				if (swap[i] == '\'') break;
				if (swap[i] == '\\') {
					/* an escaped character */
					if (swap[i+1] != 0) res[j++]=swap[++i];
				}
			}
		} else if (swap[i] == '$' && swap[i+1] == '{') {
			/* variable substitution, the variable name is delimited by a '}' */
			for (k=i+2; swap[k]!=0; k++) {
				if (swap[k]=='}') break;
			}
			if (swap[k]==0) {
				free (res);
				free (swap);
				error (ERR_EXEC, 0, "missing enclosing '}'\n");
				return NULL;
			}
			/* copy the variable name */
			exp = (char*) malloc (k-i-1);
			if (exp == NULL) {
				free (res);
				free (swap);
				error (ERR_EXEC, 0, "malloc error\n");
				return NULL;
			}
			strncpy (exp, &swap[i+2], k-i-2);
			exp[k-i-2] = 0;
			/* substitute variable */
			var = varlist_find (exp);
			free(exp);
			if (var != NULL) {
				l = strlen(var->str);
				l = l>(RESSIZE-i-1)?RESSIZE-i-1:l;
				strncpy (&res[j], var->str, l);
				j+=l;
			}
			i=k;
		} else {
			res[j++] = swap[i];
		}
	}
	res[j] = 0;
	free(swap);
	swap = (char*) malloc (strlen(res)+1);
	if (swap == NULL) {
		free (res);
		error (ERR_EXEC, 0, "malloc error\n");
		return NULL;
	}
	strcpy (swap, res);
	
/*	} else {
		swap = str;
	} */

	
/*	if ((flags & (TFL_ARITHMETIC | TFL_SUBSHELL))) { */
	for (i=0,j=0; swap[i] != 0; i++) {
		if (swap[i] == '\\') {
			/* an escaped character */
			res[j++]=swap[i];
			if (swap[i+1] != 0) res[j++]=swap[++i];
		} else if (swap[i] == '\'') {
			/* string enquoted in single quotes */
			res[j++]=swap[i];
			while (swap[++i] != 0) {
				res[j++]=swap[i];
				if (swap[i] == '\'') break;
				if (swap[i] == '\\') {
					/* an escaped character */
					if (swap[i+1] != 0) res[j++]=swap[++i];
				}
			}
		} else if (swap[i] == '$') {
			if (swap[i+1] == '(') {
				/* either a subshell execution or an arithmetic expression */
				if (swap[i+2] == '(') {
					/* arithmetic */
					for (k=i+3; swap[k]!=0; k++) {
						if (swap[k]==')' && swap[k+1]==')') break;
					}
					if (swap[k]==0) {
						free(res);
						if (swap != str) free (swap);
						error (ERR_EXEC, 0, "missing englosing '))'\n");
						return NULL;
					}
					/* copy the arithmetic expression */
					exp = (char*) malloc (k-i-2);
					if (exp == NULL) {
						free (res);
						if (swap != str) free (swap);
						error (ERR_EXEC, 0, "malloc error\n");
						return NULL;
					}
					strncpy (exp, &swap[i+3], k-i-3);
					exp[k-i-3] = 0;
					/* evaluate the expression */
					eval_expr (exp, &val); 
					free(exp);
					/* print the result to the output string */
					sprintf (buf, "%d", val);
					l = strlen(buf);
					l = l>(RESSIZE-i-1)?RESSIZE-i-1:l;
					strncpy (&res[j], buf, l);
					j+=l;
					i=k+1;
				} else {
					/* subshell */
					for (k=i+2; swap[k]!=0; k++) {
						if (swap[k]==')') break;
					}
					if (swap[k]==0) {
						free(res);
						if (swap != str) free (swap);
						error (ERR_EXEC, 0, "missing englosing ')'\n");
						return NULL;
					}
					/* copy the subshell command */
					exp = (char*) malloc (k-i-1);
					if (exp == NULL) {
						free (res);
						if (swap != str) free (swap);
						error (ERR_EXEC, 0, "malloc error\n");
						return NULL;
					}
					strncpy (exp, &swap[i+2], k-i-2);
					exp[k-i-2] = 0;
					/* TODO: run in SUBSHELL */
					free(exp);
					i=k;
				}
			} else {
				res[j++] = swap[i];
			}
		} else {
			res[j++] = swap[i];
		}
	}
	res[j] = 0;
/*	} */

	if (swap != str) free(swap);
	
	return res;
#undef RESSIZE
}


