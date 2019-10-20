#ifndef __CONST_H
#define __CONST_H

#ifdef __cplusplus
extern "C" {
#endif

/* default values of environmental variables */
#define PS1_DEFAULT "$ "
#define PS2_DEFAULT "> "

#define DBG_PRINT_TOKENS 1
#define DBG_PRINT_TRACES 2
#define DBG_PRINT_TREE 4
#define DBG_PRINT_GRAMMAR 8
#define DBG_PRINT_EXEC 16
#define DBG_PRINT_PATTERN 32
#define DBG_PRINT_GENERAL 64

extern int dbg_print;

#ifdef __cplusplus
} ; /* extern C */
#endif
	
#endif

