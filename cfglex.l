%{
/* vi: ts=4 sw=4
 * 
 * Hifs -- Handy Information For Sysadmins
 * Copyright (C) 1996,1997 Geert Jansen 
 *  
 * This is a scanner for hifs configfile. This scanner recognises, after
 * throwing away all comments, float's, int's, strings,  C-style strings and 
 * some of keywords.
 *
 * CAUTION:  Use `flex -i' to generate a case-insensitive scanner.
 */

#include "hifs.h"
#include "cfgfile.h"

char * ll_strbuf;
int ll_index, ll_size;
extern char config_file[];

void ll_strinit( void)
{
	ll_strbuf = xmalloc( (ll_size = 256));
	ll_index = 0;
}

void ll_straddchar( char c)
{
	if (ll_index == ll_size)
		ll_strbuf = xrealloc( ll_strbuf, (ll_size *= 2));
	ll_strbuf[ll_index++] = c;
}
	
%}

%option noyywrap
%option nounput
%option yylineno

%x sc1

%%

	/* 
	 * Comments are removed: 
	 */

#.*$			/* Remove comments	*/
[ \t]+			/* Remove whitespace */

	/*
	 * Single characters
	 */

[a-z]/[ \n\t]		{
						yylval.cval = yytext[0];
						return (CHAR);
					}

	/* 
	 * INT's and FLOAT's:
	 */

[0-9]+			{
					yylval.ival = strtol( yytext, NULL, 10);
					return (INT);
				}

[0-9]+\.[0-9]*	{
					yylval.dval = strtod( yytext, NULL);
					return (FLOAT);
				}

[0-9]+e[0-9]+	{
					sscanf( yytext, "%lf", &yylval.dval);
					return (FLOAT);
				} 

[0-9]+\.[0-9]*e[0-9]+	{
							sscanf( yytext, "%lf", &yylval.dval);
							return (FLOAT);
						} 

	/* 
	 * Keywords:
	 */

memory							return (MEM);
free							return (FREE);
used							return (USED);

info							return (INFO);
pid								return (PID);
cmdline							return (CMDLINE);
username						return (NAME);
priority						return (PRIO);
wchan							return (WCHAN);

sort							return (SORT);
cpu								return (CPU);
rss								return (RSS);
vsize							return (VSIZE);

mapfile							return (MAPFILE);
group							return (GROUP);
diskfree						return (DISKFREE);
delay							return (DELAY);

	/* 
	 * Un-quoted strings:
	 */

[_a-z]+[_a-z0-9]*	{
						yylval.sval = xmalloc( strlen( yytext)+1);
						strcpy( yylval.sval, yytext);
						return (STRING);	
					}

	/* 
	 * C-style, quoted strins:
	 */

\"				{ 
					BEGIN( sc1); 
					ll_strinit();
				}
<sc1>\"			{
					BEGIN( 0);
					ll_straddchar( '\000');
					yylval.sval = ll_strbuf;
					return (STRING);		
				}
<sc1>\n			{
					fprintf( stderr, "Unterminated string constant in file %s "
							"at line %d\n", config_file, yylineno);
					return (0);
				}
<sc1>\\[0-7]{1,3}		ll_straddchar( strtol( yytext, NULL, 8));
<sc1>\\x[0-9a-f]{1,2}	ll_straddchar( strtol( yytext, NULL, 16));
<sc1>\\n				ll_straddchar( '\n');
<sc1>\\t				ll_straddchar( '\t');
<sc1>\\r				ll_straddchar( '\r');
<sc1>\\v				ll_straddchar( '\v');
<sc1>\\a				ll_straddchar( '\a');
<sc1>\\b				ll_straddchar( '\b');
<sc1>\\(.|\n)			ll_straddchar( yytext[1]);
<sc1>.					ll_straddchar( yytext[0]);

	/*
	 * Single character literals
	 */

.				{
					return (yytext[0]);
				}
%%

