#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "const.h"
#include "pattern.h"

/* define a range of characters, which can be matched by ? and * symbols */
#define MIN_CHAR 1
#define MAX_CHAR 255

struct rule_ll_s {
	int srcstate;
	unsigned char in_min;
	unsigned char in_max;
	int dststate;
	int negate;
	struct rule_ll_s *next;
};

struct state_ll_s {
	int state;
	int nondet;
	struct rule_ll_s *rules;   /* list of all rules applied to given state */
	struct state_ll_s *next;
};

/* a finite state machine of a regular expression */
struct fsm_s {
	int n_states;
	int n_nondet;              /* number of non-deterministic states (* symbols) */
	struct state_ll_s *st;     /* list of all states */
};

void add_fsm_multirule (struct fsm_s *fsm, int curstate, unsigned char min, unsigned char max, int nextstate, int negative, int nondet)
{
	struct state_ll_s *st;
	struct rule_ll_s *tmp;
	if (fsm == NULL) return;
	
	/* make a new rule */
	tmp = (struct rule_ll_s*) malloc (sizeof (struct rule_ll_s));
	if (tmp == NULL) return;
	tmp->srcstate = curstate;
	tmp->in_min = min;
	tmp->in_max = max;
	tmp->dststate = nextstate;
	tmp->negate = negative;

	/* find the state, which this rule is applied to */
	st = fsm->st;
	while (st != NULL) {
		if (st->state == curstate) break;
		st = st->next;
	}
	if (st == NULL) {
		/* it's a new state */
		st = (struct state_ll_s*) malloc (sizeof (struct state_ll_s));
		if (st == NULL) {
			free (tmp);
			return;
		}
		st->state = curstate;
		st->nondet = 0;
		st->rules = NULL;
		st->next = fsm->st;
		fsm->st = st;	
	}

	fsm->n_states = (fsm->n_states > nextstate+1)?fsm->n_states:nextstate+1;
	
	/* this state is non-deterministic, we increase non-det counter */
	if (nondet && st->nondet == 0) fsm->n_nondet++;
	if (nondet) st->nondet = 1;
	/* append to this states' ruleset */
	tmp->next = st->rules;
	st->rules = tmp;
}

#define add_fsm_rule(fsm,curstate,in,nextstate,negative) add_fsm_multirule (fsm, curstate, in, in, nextstate, negative, 0)

void delete_fsm (struct fsm_s **fsm)
{
	struct state_ll_s *st, *st2;
	struct rule_ll_s *tmp, *tmp2;
	if (*fsm == NULL) return;
	st = (*fsm)->st;
	while (st != NULL) {
		tmp = st->rules;
		while (tmp != NULL) {
			tmp2 = tmp->next;
			free (tmp);
			tmp = tmp2;
		}
		st2 = st->next;
		free (st);
		st = st2;
	}
	free (*fsm);
	*fsm = NULL;
}

void dump_fsm (struct fsm_s *fsm)
{
	struct state_ll_s *st;
	struct rule_ll_s *tmp;
	if (fsm == NULL) return;
	fprintf (stdout, "FSM: %d states\n", fsm->n_states);
	st = fsm->st;
	while (st != NULL) {
		fprintf (stdout, " state %d%c\n", st->state, (st->nondet)?'*':' ');
		tmp = st->rules;
		while (tmp != NULL) {
			fprintf (stdout, "  rule %d %c[%c-%c] -> %d\n",
				tmp->srcstate,
				(tmp->negate)?'!':' ',
				(tmp->in_min < 32)?'?':(tmp->in_min > 127)?'?':tmp->in_min,
				(tmp->in_max < 32)?'?':(tmp->in_max > 127)?'?':tmp->in_max,
				tmp->dststate
			);
			tmp = tmp->next;
		}
		st = st->next;
	}
}

/* create a fsm for expression given as the argument */
struct fsm_s* make_fsm (unsigned char *expr)
{
	struct fsm_s *fsm = (struct fsm_s *) malloc (sizeof (struct fsm_s));
	int state = 0;
	int len = strlen((char*)expr);
	int index = 1;           /* index of character currently being parsed */
	int negate = 0;
	char min, max, c;
	int changestate;
	int i;

	if (fsm == NULL) return NULL;
	memset (fsm, 0, sizeof (struct fsm_s));
	if (expr == NULL) {
		free(fsm);
		return NULL;
	}
	fsm->n_states = 0;
	while (*expr != 0) {
		if (*expr == '\\') {
			/* this might be an escaping symbol */
			if (index == len) {
				/* the last char of the string */
				add_fsm_rule (fsm, state, *expr, state+1, 0);
			} else {
				add_fsm_rule (fsm, state, *(expr+1), state+1, 0);
				expr++;
				index++;
			}
			state++;
		} else if (*expr == '\'' || *expr == '"') {
			/* enquoted string, strip quotes and copy the contents */
			c = *expr++;
			index++;
			while (*expr != 0 && *expr != c) {
				if (*expr == '\\') {
					index++;
					expr++;
				}
				if (*expr == 0) break;
				/* match one single character */
				add_fsm_rule (fsm, state, *expr, state+1, 0);
				state++;
				index++;
				expr++;
			}
			if (*expr == 0) goto err;
		} else if (*expr == '?') {
			/* match any single character */
			add_fsm_multirule (fsm, state, MIN_CHAR, MAX_CHAR, state+1, 0, 0);
			state++;
		} else if (*expr == '*') {
			/* match any sequence of any characters (even empty sequence) */
/*			if (fsm->n_states == 0) {
				if this is the first rule, '*' must omit '.'
				NOTE: this will be taken care of elsewhere
				this is a bad solution:
				add_fsm_multirule (fsm, state, '.', '.', state, 1, 1);
			} else { */
			add_fsm_multirule (fsm, state, MIN_CHAR, MAX_CHAR, state, 0, 1);
/*			} */
		} else if (*expr == '[') {
			/* at first we have to make sure that there is an enclosing ] in the string */
			for (i=1; expr[i]!=0; i++) {
				if (expr[i] == '\\') {i++; continue;}
				if (expr[i] == ']') break;
			}
			if (expr[i] == 0) {
				/* only a single [ without enclosing ] */
				add_fsm_rule (fsm, state, *expr, state+1, 0);
				state++;
				expr++;
				index++;
				continue;
			}
			/* match any of the characters in square braces (or neither of them, if first char is ^) */
			expr++;
			index++;
			changestate = 0;
			if (index > len) goto err;
			/* check whether the first character in braces is ^ */
			if (*expr == '^') {
				negate = 1;
				expr++;
				index++;
				if (index > len) goto err;
			} else negate = 0;
			while (index <= len) {
				if (*expr == ']') break;  /* closing bracket was found */
				else if (*expr == '\\') {
					/* some escaping needs to be done */
					min = *(++expr);
					index++;
				} else min = *expr;
				if (index > len) goto err;
				/* check for a '-' */
				if ((index < (len-1)) && (*(expr+1) == '-')) {
					/* is this '-' in the middle, or the last char of the braces? */
					if (*(expr+2) != ']') {
						if ((index < (len-2)) && (*(expr+2) == '\\')) {
							max = *(expr + 3);
							expr++;
							index++;
						} else {
							max = *(expr + 2);
						}
						add_fsm_multirule (fsm, state, min, max, state + 1, negate, 0);
						changestate = 1;
						expr+=2;
						index+=2;
					} else {
						add_fsm_rule (fsm, state, min, state + 1, negate);
						changestate = 1;
					}
				} else {
					add_fsm_rule (fsm, state, min, state + 1, negate);
					changestate = 1;
				}
				expr++;
				index++;
			}
			if (*expr != ']') goto err;
			if (changestate) state++;
		} else {
			/* match one single character */
			add_fsm_rule (fsm, state, *expr, state+1, 0);
			state++;
		}
		expr++;
		index++;
	}

	return fsm;
err:
	delete_fsm (&fsm);
	return NULL;
}

struct match_stack_s {
	struct state_ll_s *state;    /* the state where the non-determinism occurs */
	int index;                   /* index to character, which corresponds with the given state */
	int retries;                 /* number of rules already tried in the current state */
	struct match_stack_s *next;
};

/* match str against given non-deterministic fsm (accepting only in the last state and without epsilon rules) */
int match (struct fsm_s *fsm, unsigned char *str)
{
	int state = 0;
	int index = 0;
	int len = strlen ((char*)str);
	struct match_stack_s *stack = NULL, *tmp;
	struct state_ll_s* st;
	struct rule_ll_s* rule;
	int retries=0;
	int c;
	
	if (fsm == NULL || str == NULL) return 0;

/* fprintf (stderr, "?matching %s\n", str); */

	
retry:
	while (index < len) {
		/* find a suitable rule(s) for the current state (state) and the current input character (str[index]) */
		st = fsm->st;
		while ((st != NULL) && (st->state != state)) {
			st = st->next;
		}
		if (st == NULL) {
			/* this would be an error */
/* fprintf (stderr, "*no state info found on %d\n", state); */
			break;
		}
/* fprintf (stderr, "?found state %d\n", state); */
		if (st->nondet && retries == 0) {
			/* it's a non-deterministic state, we need to push our current state on the stack */
			/* this is applied only when iterating through the non-deterministic state for the first time */
/* fprintf (stderr, "?passing thru a nondet state %d\n", state); */
			tmp = (struct match_stack_s*) malloc (sizeof (struct match_stack_s));
			if (tmp == NULL) {
				/* delete stack */
				while (stack != NULL) {
					tmp = stack->next;
					free (stack);
					stack = tmp;
				}
				return 0;
			}
			tmp->state = st;
			tmp->index = index;
			tmp->retries = 0;
			tmp->next = stack;
			stack = tmp;
			retries = 0;
		}
		/* find next rule applicable in current situation */
		c = 0;
		rule = st->rules;
		while (rule != NULL) {
			if ((rule->negate == 0) && (rule->in_min <= str[index]) && (rule->in_max >= str[index])) {
				c++;
				if (c > retries) break;
			} else if ((rule->negate == 1) && ((rule->in_min > str[index]) || (rule->in_max < str[index]))) {
				c++;
				if (c > retries) break;
			}
			rule = rule->next;
		}
		if (c <= retries) {
			/* no applicable rule was found */
/* fprintf (stderr, "*no applicable rule for state %d and input '%c' c %d retries %d\n", state, str[index], c, retries); */
			break;
		}
		/* apply the rule */
/* fprintf (stdout, "?taking rule %d %c[%c-%c] -> %d\n", 
	rule->srcstate,
	(rule->negate)?'!':' ',
	(rule->in_min < 32)?'?':(rule->in_min > 127)?'?':rule->in_min,
	(rule->in_max < 32)?'?':(rule->in_max > 127)?'?':rule->in_max,
	rule->dststate
); */
		state = rule->dststate;
		
		retries = 0;
		index++;
	}
	
	if (state == (fsm->n_states - 1) && index == len) {
		/* only the last state is the accepting one */
		/* delete stack */
/* fprintf (stderr, "!accepted\n"); */
		while (stack != NULL) {
			tmp = stack->next;
			free (stack);
			stack = tmp;
		}
		return 1;
	}

	if (stack != NULL) {
		/* try another path through the non-deterministic FSM */
		if (st != NULL && st->nondet) {
			/* pop the last item on stack */
/* fprintf (stderr, "*stack pop\n"); */
			tmp = stack->next;
			free (stack);
			stack = tmp;
		}
		if (stack == NULL) {
			/* end -> no match */
/* fprintf (stderr, "!stack empty -> reject\n"); */
			return 0;
		}
		/* return to previous non-deterministic state and try another path */
		stack->retries++;
		state = stack->state->state;
		index = stack->index;
		retries = stack->retries;
/* fprintf (stderr, "*retrying from state %d index %d char %c retries %d\n", state, index, str[index], retries); */
		
		goto retry;
	}

	/* stack is empty and no path was found -> no match */
/* fprintf (stderr, "!just rejected\n"); */
	return 0;
}

int pattern_filename_gen (char *pat, struct llist_s **q)
{
	struct fsm_s *fsm=NULL;
	DIR *d=NULL;
	struct dirent *ent;
	int matches = 0;
	char *tmp,*dir,*cnsdir=".",*p;
	int i,j,recur=0,l,part=0;
	struct stat s;
	
	if (pat == NULL || pat[0] == 0) return 0;
	
	/* the path may contain '/', take all possibilities into account */
	for (i=-1,j=0,l=0; pat[j]!=0; j++) {
		if (pat[j] == '*' || pat[j] == '?' || (pat[j]==']' && part)) l = 1;
		else if (pat[j] == '[') part = 1;
		else if (pat[j] == '/') {
			part = 0;
			if (l==1) {
				break;
			} else {
				i=j;
			}
		}
	}
	
	if (i == -1 && pat[j] == 0) {
		/* the pattern contains no '/'
		 * e.g. "*foo?" */
		p = pat;
		dir = cnsdir;
	} else if (i == -1 && pat[j] != 0) {
		/* e.g. "[a-z]/bar" */
		recur = 1;
		dir = cnsdir;
		p = (char*) malloc (j+1);
		if (p == NULL) {
			error (ERR_PATTERN, 0, "internal error - malloc error\n");
			return 0;	
		}
		strncpy (p, pat, j);
		p[j]=0;
	} else if (i > -1 && pat[j] == 0) {
		/* reached the end of the path string, no further recursion
		 * e.g. "/dir/a*" */
		p = &pat[i+1];
		dir = (char*) malloc (i+2);
		if (dir == NULL) {
			error (ERR_PATTERN, 0, "internal error - malloc error\n");
			return 0;
		}
		strncpy (dir, pat, i+1);
		dir[i+1] = 0;		
	} else {
		/* extract path and the first pattern from pat, will require recursion
		 * e.g. "/dir/a?/aaa" */
		recur = 1;
		p = (char*) malloc (j-i);
		if (tmp == NULL) {
			error (ERR_PATTERN, 0, "internal error - malloc error\n");
			return 0;
			
		}
		strncpy (p, &pat[i+1], j-i-1);
		p[j-i-1] = 0;
		dir = (char*) malloc (i+2);
		if (dir == NULL) {
			free (p);
			error (ERR_PATTERN, 0, "internal error - malloc error\n");
			return 0;
		}
		strncpy (dir, pat, i+1);
		dir[i+1] = 0;
	}

	if (p[0]==0) {
		/* simple directory case */
		tmp = (char*) malloc (strlen(dir)+1);
		if (tmp == NULL) {
			error (ERR_PATTERN, 0, "internal error - malloc error\n");
			goto quit;
		}
		strcpy (tmp, dir);
		*q = llist_insert_alphabet (*q, tmp, &free);
		matches = 1;
		goto quit;
	}
	
	/* construct a finite state machine matching given pattern */
	error (ERR_DBG_PRINT_PATTERN, 1, "matching pattern \"%s\" in dir \"%s\"\n", p, dir);
	error (ERR_DBG_PRINT_PATTERN, 1, "i=%d, j=%d, recur=%d\n", i, j, recur);
	fsm = make_fsm ((unsigned char*)p);
	if (fsm == NULL) {
		if (dir != NULL && dir != cnsdir) free (dir);
		if (recur && p != NULL) free (p);
		error (ERR_PATTERN, 0, "error parsing pattern\n");
		return 0;
	}

	if (dbg_print & DBG_PRINT_PATTERN)
		dump_fsm(fsm);
	
	/* use the fsm and try to match all files in current directory */
	l = strlen (dir);
	d = opendir (dir);
	while (d != NULL) {
		ent = readdir (d);
		if (ent == NULL) break;
		if (ent->d_name[0] == '.' && (p[0]=='?' || p[0]=='*')) continue;  /* special case: skip '.' at the beginning of the filename */
		if (match (fsm, (unsigned char*)ent->d_name)) {
			error (ERR_DBG_PRINT_PATTERN, 1, "match \"%s\"\n", ent->d_name);
			
			if (recur) {
				/* handle recursion on directories */
				if (dir == cnsdir) {
					/* omit default directory, i.e. "./" */
					tmp = (char*) malloc (strlen(ent->d_name) + 1 + strlen(&pat[j]));
					if (tmp == NULL) {
						error (ERR_PATTERN, 0, "internal error - malloc error\n");
						continue;
					}
					strcpy (tmp, ent->d_name);
				} else {
					/* concatenate directory and file */
					tmp = (char*) malloc (strlen(ent->d_name) + 1 + l + strlen(&pat[j]));
					if (tmp == NULL) {
						error (ERR_PATTERN, 0, "internal error - malloc error\n");
						continue;
					}
					sprintf (tmp, "%s%s", dir, ent->d_name);
				}
				if ((stat (tmp, &s) == 0) && (S_ISDIR(s.st_mode))) {
					/* tmp points to a directory */
					strcat (tmp, &pat[j]);
					matches += pattern_filename_gen (tmp, q);
				}
				free (tmp);
			} else {
				/* enqueue this file as it matches given pattern */
				if (dir == cnsdir) {
					/* omit default directory, i.e. "./" */
					tmp = (char*) malloc (strlen(ent->d_name)+1);
					if (tmp == NULL) {
						error (ERR_PATTERN, 0, "internal error - malloc error\n");
						continue;
					}
					strcpy (tmp, ent->d_name);
				} else {
					/* concatenate directory and file */
					tmp = (char*) malloc (strlen(ent->d_name)+1 + l);
					if (tmp == NULL) {
						error (ERR_PATTERN, 0, "internal error - malloc error\n");
						continue;
					}
					sprintf (tmp, "%s%s", dir, ent->d_name);
				}
				*q = llist_insert_alphabet (*q, tmp, &free);
				matches++;
			}
		}
		
	}

quit:	
	if (d != NULL) closedir (d);
	
	if (dir != NULL && dir != cnsdir) free (dir);
	if (recur && p != NULL) free (p);
	
	if (fsm != NULL) delete_fsm (&fsm);
	error (ERR_DBG_PRINT_PATTERN, 1, "%d files matched\n", matches);
	return matches;
}

/* returns 1 if string needle matches the pattern pat
 * 0 otherwise */
int pattern_match (char *pat, char *needle)
{
	struct fsm_s *fsm;

	error (ERR_DBG_PRINT_PATTERN, 1, "matching pattern \"%s\"\n", pat);
	fsm = make_fsm ((unsigned char*)pat);
	if (fsm == NULL) {
		error (ERR_PATTERN, 0, "error parsing pattern\n");
		return 0;
	}

	if (match (fsm, (unsigned char*)needle)) {
		delete_fsm (&fsm);
		error (ERR_DBG_PRINT_PATTERN, 1, "\"%s\" matches\n", needle);
		return 1;
	}

	error (ERR_DBG_PRINT_PATTERN, 1, "\"%s\" doesn't match\n", needle);
	delete_fsm (&fsm);
	return 0;
}

