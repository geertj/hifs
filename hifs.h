/* vi: ts=4 sw=4
 * 
 * Hifs -- Handy Information For Sysadmins
 * Copyright (C) 1996,1997 Geert Jansen 
 *  
 * Handy Information For Sysadmins */

#ifndef _HIFS_H
#define _HIFS_H

#define HIFS_MAJOR		1	/* Major verion number */
#define HIFS_MINOR		4	/* Minor verion number */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE		/* Enable GNU extensions (glibc) */
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>		/* Autoconf's output */
#else
#error Where is config.h ???
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/utsname.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utmp.h>
#include <string.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <limits.h>
#include <paths.h>
#include <math.h>
#include <getopt.h>

#if defined (HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined (HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#elif defined (HAVE_CURSES_H)
#include <curses.h>
#else
#error Where is (n)curses.h ???
#endif

#if defined (CONFIG_SHADOW)
#include <shadow.h>
#endif 

/* Sort/info/mem/kill modes */

#define SORT_CPU 			0
#define SORT_RSS 			1
#define SORT_VSIZE			2
#define SORT_LAST			2

#define INFO_PID			0
#define INFO_CMDLINE		1
#define INFO_WCHAN			2
#define INFO_NAME			3
#define INFO_PRIO			4
#define INFO_LAST			4

#define KILL_NICE			0
#define KILL_BRUTE			1

#define MEM_FREE			0
#define MEM_USED			1
#define MEM_LAST			1

/* The cpu usage that we show is a weighted average over the usage during the
 * last three periods. We take an exponential decay for the weighting factors. 
 * The factors are normalized at 100 (%) */

#define WEIGHT_1			66.53
#define WEIGHT_2			24.47
#define WEIGHT_3			9.00

/* Priorities for messages */

#define MAX_PRIO			3	
#define MED_PRIO			2
#define MIN_PRIO			1

/* Various maxima's/minima's */

#define MAX_GROUPS			2
#define MAX_GROUPNAME		32
#define MAX_GROUPMEMBERS	8
#define MAX_HOSTNAME		8
#define MAX_SHOWPROCESSES	12

/* Screen related constants */

#define SCREEN_WIDTH		26
#define SCREEN_HEIGHT		24

#define STAT_NOTLOGGEDIN	0
#define	STAT_JUSTLOGGEDIN	1
#define STAT_LOGGEDIN		2

#define DEF_TIMEOUT 		30	
#define BIG_SLEEP			1000

#define EMPTY "                          "

/* The geometry of the window */

#define X_TITLE				0
#define Y_TITLE				0
#define X_LOGINS			9
#define Y_LOGINS			4
#define X_LOGINSR			23
#define Y_LOGINSR			4
#define X_XLOGINS			9
#define Y_XLOGINS			5
#define X_XLOGINSR			23
#define Y_XLOGINSR			5
#define X_LOAD1				6
#define Y_LOAD1				1
#define X_LOAD2				13
#define Y_LOAD2				1
#define X_LOAD3				21
#define Y_LOAD3				1
#define X_MEM				5
#define Y_MEM				3
#define X_SWAP				19
#define Y_SWAP				3
#define X_CPUU				4
#define Y_CPUU				2
#define X_CPUS				11
#define Y_CPUS				2
#define X_CPUI				19
#define Y_CPUI				2
#define X_GROUP				18
#define Y_GROUP				6
#define X_PROCESSES_1		0
#define X_PROCESSES_2		9
#define X_PROCESSES_3		17
#define Y_PROCESSES			9
#define X_FLAGS				0
#define Y_FLAGS				8
#define X_MESSAGES			0
#define Y_MESSAGES			23
	
/* libc 5 does not define this */

#ifndef CTRL
#define CTRL(c) ((c)&31)
#endif

/* Definitions from proc.c */

extern struct cpu_info			cpu;		/* CPU state info		*/
extern struct mem_info			mem;		/* Memory info			*/
extern struct utmp *			logins;		/* utmp array			*/
extern struct process_info *	procs;		/* process table		*/
extern double 					loads[];	/* load averages		*/

extern int nlogins;			/* # of entries in utmp 				*/

extern int procs_maxi;		/* Max index in process table			*/

int 		proc_init			(void);
void 		proc_update			(int);
void		proc_close			(void);

/* Definitions from screen.c: */

int			screen_init			(int);
void		screen_setup		(void);
void 		screen_update		(void);
void		screen_close		(void);
void		show_help			(void);

void		let_user_kill		(int);
void		let_user_write		(void);
void		let_user_renice		(void);

char *		get_string			(const char *, char);
void		queue_msg			(int, const char *, ...);
void		notice				(const char *, ...);
void		xsleep				(int);
int			xgetch				(int,int);

extern int nmessages;		/* # of messages in message queue		*/

extern struct msg_entry *		messages;	/* message queue		*/

/* Definitions from util.c: */

void *		xmalloc( size_t);
void *		xrealloc( void *, size_t);
char *		strnzcpy( char *, const char *, size_t);

/* Definitions from hifs.c: */

extern int			memory;
extern int			info;
extern int			sort;
extern int			min_diskfree;

extern int			warned;
extern char *		mapfile;
extern int			rootflag;
extern char *		user;
extern int			version_code;
extern char *		version_string;

extern struct group *	groups;
extern int				ngroups;
extern int				mlgroups;
extern double			delay;

extern struct mode		sortmodes[];
extern struct mode		infomodes[];
extern struct mode		memmodes[];

/* Various proc related data structures */

#define PINFO_COMM_SIZE			32
#define PINFO_CMDLINE_SIZE		32
#define	PINFO_USER_SIZE			16
#define PINFO_WCHAN_SIZE		32

struct process_info {
	int 			pid;
	int 			serial;
	char 			comm[PINFO_COMM_SIZE];
	char 			cmdline[PINFO_CMDLINE_SIZE];
	char			user[PINFO_USER_SIZE];
	
	int				uid, euid, suid, fsuid;
	int				gid, egid, sgid, fsgid;

	char 			state;
	double	 		pct_cpu;	/* Mean CPU usage over last periods */
	double			times[8];	/* Last 8 CPU usages */
	int 			index;		/* Index in the times field	*/
	unsigned long 	jiffies;	/* Absolute # jiffies the process has used */
	long int 		priority;
	unsigned long 	vsize;		/* vsize  */
	long int 		rss;		/* Resident Set Size	*/
	unsigned long	wchan;
	char 			strwchan[PINFO_WCHAN_SIZE];
};

struct cpu_info {
	int 			index;
	double	 		pct_idle;
	double			idle[8];
	unsigned long 	idlejiffies;
	double	 		pct_system;
	double	 		system[8];
	unsigned long 	systemjiffies;
	double	 		pct_user;
	double	 		user[8];
	unsigned long 	userjiffies;
	double			pct_nice;
	double			nice[8];
	unsigned long	nicejiffies;
};

#define MSG_TEXT_SIZE		64

struct msg_entry {
	int prio;
	char text[MSG_TEXT_SIZE];
};

struct mem_info {
	unsigned long	total, used, free, shared, buffers, cached;
	unsigned long	swaptotal, swapused, swapfree;
};
 
#define WCHAN_STR_SIZE 		32

struct wchan_entry {
	unsigned long	wchan;
	char			strrep[WCHAN_STR_SIZE];
};

/* Group related data structures */

struct grp_member {
	char id;
	char * name;
	int status;
	int timeout;
};

struct group {
	char * name;
	int nmembers;
	int mlmembers;
	struct grp_member * members;
};

struct mode {
	char l[32];
	char s[8];
};

#endif 	/* ! _HIFS_H */
