%{
/* vi: ts=4 sw=4
 * 
 * Hifs -- Handy Information For Sysadmins
 * Copyright (C) 1996,1997 Geert Jansen 
 *  
 * cfgfile.y: This is a bison parser for the hifs configfile. 
 *
 * CAUTION:   Use `bison -d' to generate a header file.
 */

#include "hifs.h"

struct group grp = {NULL, 0, 0, NULL};

/* Stuff exported by the lexer */

extern int yylex( void);
extern int yylineno;

void	yyerror			(char *);
void 	yy_group_add	(char *, char);
void	yy_group_finish	(char *);

%}

/* ------------------------------------------------------------------------
 * Bison declarations
 */

%union {
	char cval;
	int ival;
	double dval;
	char * sval;
}

%token MEM FREE USED INFO PID CMDLINE NAME PRIO WCHAN
%token SORT CPU RSS VSIZE MAPFILE GROUP DELAY DISKFREE

%token <cval> CHAR
%token <ival> INT
%token <dval> FLOAT
%token <sval> STRING

%type  <dval> float

%%

/* ------------------------------------------------------------------------
 * Grammar rules and actions
 */

input:	/* Emtpy */
		| input line
;

float:	  FLOAT						{ $$ = $1; }
		| INT						{ $$ = (double) $1; }
;

line:	SORT CPU					{ sort = SORT_CPU; }
		| SORT RSS					{ sort = SORT_RSS; }
		| SORT VSIZE				{ sort = SORT_VSIZE; }
		| INFO PID					{ info = INFO_PID; }
		| INFO NAME					{ info = INFO_NAME; }
		| INFO CMDLINE				{ info = INFO_CMDLINE; }
		| INFO PRIO					{ info = INFO_PRIO; }
		| INFO WCHAN				{ info = INFO_WCHAN; }
		| MEM FREE					{ memory = MEM_FREE; }
		| MEM USED					{ memory = MEM_USED; }
		| MAPFILE STRING			{ mapfile = $2; } 
		| DELAY float				{ delay = $2; }
		| DISKFREE INT				{ min_diskfree = $2; }
		| GROUP STRING '{' gmember '}'	{ yy_group_finish( $2); }
;

gmember:	/* Emtpy */
			| gmember STRING ',' CHAR	{ yy_group_add( $2, $4); } 
;

%%

/* ------------------------------------------------------------------------
 * Additional C code
 */

void yyerror( char * s)
{
	fprintf( stderr, "Error form the parser: `%s' at line %d\n", 
	         s, yylineno);
}

void yy_group_add( char * name, char id)
{
	int i;

	if (!grp.nmembers) {
		grp.mlmembers = 8;
		grp.members = xmalloc( 8 * sizeof (struct grp_member));
	}
	if (grp.nmembers == grp.mlmembers)
		grp.members = xrealloc( grp.members, (grp.mlmembers *= 2) * sizeof 
				(struct grp_member));
	i = grp.nmembers++;
	grp.members[i].name = name;
	grp.members[i].id = id;
	grp.members[i].status = STAT_LOGGEDIN;
	grp.members[i].timeout = 0;
}

void yy_group_finish( char * name)
{
	int i;

	if (!ngroups) {
		mlgroups = 4;
		groups = xmalloc( 4 * sizeof (struct group));
	}
	if (ngroups == mlgroups)
		groups = xrealloc( groups, (mlgroups *= 2) * sizeof (struct group));

	i = ngroups++;
	groups[i].name = name;
	groups[i].nmembers = grp.nmembers;
	groups[i].mlmembers = grp.mlmembers;
	groups[i].members = grp.members;

	grp.nmembers = grp.mlmembers = 0;
	grp.members = NULL;
}

