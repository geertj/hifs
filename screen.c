/* vi: ts=4 sw=4
 * 
 * Hifs -- Handy Information For Sysadmins
 * Copyright (C) 1996,1997 Geert Jansen 
 */
  
#include "hifs.h"

/* ------------------------------------------------------------------------
 * Globals. */

char	hostname[MAX_HOSTNAME+1];		/* The hostname	*/
char	Hostname[MAX_HOSTNAME+1];		/* Niced hostname */

int 	nprocs;					/* # of processes to show			*/
int		pids[32];				/* Pids of processes to show		*/

int 	nmessages		= 0;	/* # of messages in message queue 	*/
int		messages_size	= 32;	/* initial message table size		*/

struct msg_entry * 		messages	= NULL;		/* messages			*/

/* ------------------------------------------------------------------------
 * Function prototypes not defined in hifs.h */

void		show_loads			(void);
void		show_mem			(void);
void		show_cpu			(void);
void		show_logins			(void);
void		show_groups			(void);
void		show_messages		(void);
void		show_flags			(void);
int			logged_in			(const char *);
void		sort_procs			(void);

int			select_process		(void);

void		msg					(const char * fmt, ...);
void		title				(const char * fmt, ...);

/* ------------------------------------------------------------------------
 * sort_procs: Get the top MAX_SHOWPROCESSES of the processes.  */

void sort_procs( void) 
{
	int i, j, k, l;
	unsigned long umin;
	double dmin;

	/* We get the top MAX_SHOWPROCESSES of the processes using a kind of
	 * selectionsort. This algorithm should be fast when a) the array to
	 * sort may not be touched, b) the key is small compared to the record, 
	 * and c) MAX_SHOWPROCESSES is small compared to the number of records. 
	 * The array is not touched, the first MAX_SHOWPROCESSES pids
	 * are stored into the array 'pids'. */

	/* Find the 0..MAX_SHOWPROCESSES highest maximums */

	for (i=0; i<MAX_SHOWPROCESSES; i++) {
		dmin = umin = k = 0;
		for (j=0; j<procs_maxi; j++) {
			if (!procs[j].pid)
				continue;
			switch (sort) {
			case SORT_CPU:
				if (procs[j].pct_cpu > dmin) {
					for (l=0; (l < i) && (procs[j].pid != pids[l]); l++);
					if (l != i)
						break;
					dmin = procs[j].pct_cpu;
					k = j;
				}
				break;
			case SORT_RSS:
				if (procs[j].rss > umin) {
					for (l=0; (l < i) && (procs[j].pid != pids[l]); l++);
					if (l != i)
						break;
					umin = procs[j].rss;
					k = j;
				}
				break;
			case SORT_VSIZE:
				if (procs[j].vsize > umin) {
					for (l=0; (l < i) && (procs[j].pid != pids[l]); l++);
					if (l != i)
						break;
					umin = procs[j].vsize;
					k = j;
				}
				break;
			}
		}
		pids[i] = procs[k].pid;

		if (!dmin && !umin)
			break;
	}
	pids[i] = 0;
	nprocs = i;

	return;
}
		
/* ------------------------------------------------------------------------
 * logged_in: Return nonzero if user is logged in.  */

int logged_in( const char * user)
{
	int i;

	for (i=0; i<nlogins; i++)
		if (!(strncmp( logins[i].ut_user, user, UT_NAMESIZE)))
			return (1);
	return (0);
}
	
/* ------------------------------------------------------------------------
 * show_procs: Output the processes to the screen, in a nice layout.  */

void show_procs( void)
{
	int i, j;
	char buf[32];

	for (i=0; pids[i]; i++) {

		for (j=0; (j < procs_maxi) && (procs[j].pid != pids[i]); j++);
		if (j == procs_maxi)
			continue;

		/* column 1: process name */

		mvprintw( Y_PROCESSES+i, X_PROCESSES_1, "%-8.8s ", procs[j].comm);

		/* column 2: process info, sorted on */

		switch (sort) {
		case SORT_CPU:
			mvprintw( Y_PROCESSES+i, X_PROCESSES_2, "%4.1f%% %c ", 
					procs[j].pct_cpu, procs[j].state);
			break;
		case SORT_RSS:
			if (procs[j].rss >> 20) 
				sprintf( buf, "%4.1fM", (double) procs[j].rss / (1024*1024));
			else
				sprintf( buf, "%4luK", procs[j].rss >> 10);
			mvprintw( Y_PROCESSES+i, X_PROCESSES_2, "%s %c ", buf, 
					procs[j].state);
			break;
		case SORT_VSIZE:
			if (procs[j].vsize >> 20) 
				sprintf( buf, "%4.1fM", (double) procs[j].vsize / (1024*1024));
			else
				sprintf( buf, "%4luK", procs[j].vsize >> 10);
			mvprintw( Y_PROCESSES+i, X_PROCESSES_2, "%s %c ", buf, 
					procs[j].state);
			break;
		}

		/* column 3: extra process info */

		switch (info) {
		case INFO_PID:
			mvprintw( Y_PROCESSES+i, X_PROCESSES_3, "%-9d", procs[j].pid);
			break;
		case INFO_CMDLINE:
			mvprintw( Y_PROCESSES+i, X_PROCESSES_3, "%-9.9s", procs[j].cmdline);
			break;
		case INFO_WCHAN:
			mvprintw( Y_PROCESSES+i, X_PROCESSES_3, "%-9.9s", 
					procs[j].strwchan);
			break;
		case INFO_NAME:
			mvprintw( Y_PROCESSES+i, X_PROCESSES_3, "%-9.9s", procs[j].user);
			break;
		case INFO_PRIO:
			mvprintw( Y_PROCESSES+i, X_PROCESSES_3, "%-9d", procs[j].priority);
			break;
			
		}
	
	}

	for (; i<MAX_SHOWPROCESSES; i++)
		mvprintw( Y_PROCESSES+i, X_PROCESSES_1, EMPTY); 
}

/* ------------------------------------------------------------------------
 * show_logins: Show the number of tty-logins/x-logins.  */

void show_logins( void)
{
	int tlogins, tloginsr, xlogins, xloginsr;
	int i, a, b;

	tlogins = tloginsr = xlogins = xloginsr = a = b = 0;
	for (i=0; i<nlogins; i++) {
		a = tlogins; b = xlogins;
		if (strchr( logins[i].ut_host, ':')) 
			xlogins++; 
		else 
			tlogins++; 
		while ((i+1 < nlogins) && (!strncmp( logins[i].ut_user, 
		      logins[i+1].ut_user, UT_NAMESIZE))) {
			i++;
			if (strchr( logins[i].ut_host, ':')) 
				xlogins++; 
			else 
				tlogins++; 
		}
		if (a != tlogins)
			tloginsr++;
		if (b != xlogins)
			xloginsr++;
	}

	mvprintw( Y_LOGINS, X_LOGINS, "%3d", tlogins);
	mvprintw( Y_LOGINSR, X_LOGINSR, "%3d", tloginsr);
	mvprintw( Y_XLOGINS, X_XLOGINS, "%3d", xlogins);
	mvprintw( Y_XLOGINSR, X_XLOGINSR, "%3d", xloginsr);
}


/* ------------------------------------------------------------------------
 * show_groups: Show user group information.   */

void show_groups( void)
{
	int i, j;

	for (i=0; (i < MAX_GROUPS) && (i < ngroups); i++) {
		for (j=0; (j < groups[i].nmembers) && (j < MAX_GROUPMEMBERS); j++) {
			if (logged_in( groups[i].members[j].name)) {
				switch (groups[i].members[j].status) {
				case STAT_NOTLOGGEDIN:
					groups[i].members[j].status = STAT_JUSTLOGGEDIN;
					groups[i].members[j].timeout = DEF_TIMEOUT;
					break;
				case STAT_JUSTLOGGEDIN:
					if (!groups[i].members[j].timeout) 
						groups[i].members[j].status = STAT_LOGGEDIN;
					else
						(groups[i].members[j].timeout)--;
					break;
				}
			} else
				groups[i].members[j].status = STAT_NOTLOGGEDIN;
		}
	}

	for (i=0; (i < MAX_GROUPS) && (i < ngroups); i++) {
		move( Y_GROUP+i, X_GROUP);
		for (j=0; (j < groups[i].nmembers) && (j < MAX_GROUPMEMBERS); j++) {
			switch (groups[i].members[j].status) {
			case STAT_NOTLOGGEDIN:
				addch( ' ');
				break;
			case STAT_JUSTLOGGEDIN:
				attrset( A_BOLD);
				addch( groups[i].members[j].id);
				attrset( 0);
				break;
			case STAT_LOGGEDIN:
				addch( groups[i].members[j].id);
				break;
			}
		}
	}
}

/* ------------------------------------------------------------------------
 * show_loads: Show the system's load averages.  */

void show_loads( void)
{
	mvprintw( Y_LOAD1, X_LOAD1, "%5.2f", loads[0]);
	mvprintw( Y_LOAD2, X_LOAD2, "%5.2f", loads[1]);
	mvprintw( Y_LOAD3, X_LOAD3, "%5.2f", loads[2]);
}


/* ------------------------------------------------------------------------
 * show_mem: Show free/used memory/swap.  */

void show_mem( void) {
	switch (memory) {
	case MEM_FREE:
		mvprintw( Y_MEM, X_MEM, "%6dK", mem.free >> 10);
		mvprintw( Y_SWAP, X_SWAP, "%6dK", mem.swapfree >> 10);
		break;
	case MEM_USED:
		mvprintw( Y_MEM, X_MEM, "%6dK", mem.used >> 10);
		mvprintw( Y_SWAP, X_SWAP, "%6dK", mem.swapused >> 10);
		break;
	}
}

/* ------------------------------------------------------------------------
 * show_cpu: Show cpu statistics.  */

void show_cpu( void)
{
	mvprintw( Y_CPUU, X_CPUU, "%5.1f%%U", cpu.pct_user + cpu.pct_nice); 
	mvprintw( Y_CPUS, X_CPUS, "%5.1f%%S", cpu.pct_system); 
	mvprintw( Y_CPUI, X_CPUI, "%5.1f%%I", cpu.pct_idle); 
}

/* ------------------------------------------------------------------------
 * show_messages: Clear the message queue and show the most important 
 * message */

void show_messages( void)
{
	int i, maxv, maxi;

	msg("");
	if (!nmessages) 
		return;
	maxi = 0;
	maxv = messages[0].prio;
	for (i=1; i<nmessages; i++) {
		if (messages[i].prio > maxv) {
			maxv = messages[i].prio;
			maxi = i;
		}
	}
	if (maxv > MIN_PRIO)
		attrset( A_BOLD);
	msg( messages[maxi].text);
	attrset( 0);
	nmessages = 0;
}

/* ------------------------------------------------------------------------
 * show_flags: Show misc flags to the user. */

void show_flags( void)
{
	char str[32];

	sprintf( str, "--%s-%s-%s-------%s--", sortmodes[sort].s, 
			infomodes[info].s, memmodes[memory].s, 
			rootflag ? "ROOT" : "----");
	mvaddstr( Y_FLAGS, X_FLAGS, str);
}

/* ------------------------------------------------------------------------
 * screen_update: Update the screen, external entry point. */

void screen_update( void) 
{
	title( "Information for %s", Hostname);
	sort_procs();
	show_procs();
	show_cpu();
	show_loads();
	show_mem();
	show_logins();
	show_groups();
	show_flags();
	show_messages();
	refresh();
}

/* ------------------------------------------------------------------------
 * msg: Print a message to the screen. */

void msg( const char * fmt, ...)
{
	char buf[BUFSIZ];
	va_list args;

	va_start( args, fmt);
	vsprintf( buf, fmt, args);
	va_end( args);

	mvaddstr( Y_MESSAGES, X_MESSAGES, EMPTY);
	mvaddnstr( Y_MESSAGES, X_MESSAGES, buf, SCREEN_WIDTH);
}
	
/* ------------------------------------------------------------------------
 * queue_msg: Put a message in the message queue. */

void queue_msg( int prio, const char * fmt, ...)
{
	int i;
	va_list args;

	if (nmessages == messages_size)
		messages = xrealloc( messages, (messages_size *= 2) * sizeof
				(struct msg_entry));

	va_start( args, fmt);
	i = nmessages++;
	messages[i].prio = prio;
	vsnprintf( messages[i].text, MSG_TEXT_SIZE, fmt, args);
	va_end( args);
}

/* ------------------------------------------------------------------------
 * notice: Notice the user of some event. The string 'str' is shown 1 second
 * or till a keypress. */

void notice( const char * fmt, ...)
{
	char buf[BUFSIZ];
	va_list args;

	va_start( args, fmt);
	vsprintf( buf, fmt, args);
	va_end( args);

	msg( buf);
	refresh(); xgetch( 10, 1);
	msg( "");
}

/* ------------------------------------------------------------------------
 * title: Set the title of the hifs window. */

void title( const char * fmt, ...)
{
	int len;
	char buf[BUFSIZ];
	va_list args;
	
	va_start( args, fmt);
	len = vsprintf( buf, fmt, args);
	va_end( args);

	attrset( A_REVERSE);
	mvaddstr( Y_TITLE, X_TITLE, EMPTY);
	mvaddstr( Y_TITLE, X_TITLE + (SCREEN_WIDTH - len) / 2, buf);
	attrset( 0);
}

/* ------------------------------------------------------------------------
 * get_string: Prompt the user for input. 'prompt' is displayed. This 
 * contains a simple line editor. */

char * get_string( const char * prompt, char c)
{
	int len, key, scrsize;
	int str_cur, str_end;
	int scr_beg;
	int disp_start;
	int done;
	int i;

	static char str[256];

	len = strlen( prompt);
	scrsize = SCREEN_WIDTH - len - 1;
	if (scrsize <= 4)
		return (NULL);
	str_cur = str_end = 0;
	scr_beg = X_MESSAGES + len + 1;
	
	move( Y_MESSAGES, X_MESSAGES);
	attrset( 0);
	addstr( prompt);
	addch( ' ');
	for (i=0; i<scrsize; i++) 
		addch( ' ' | A_REVERSE);
	move( Y_MESSAGES, scr_beg);
	refresh();

	done = 0;
	while (!done) {
		key = xgetch( BIG_SLEEP, 1);
		switch (key) {
		case KEY_DC:
			if (str_cur != str_end) {
				memmove( str+str_cur, str+str_cur+1, str_end-str_cur);
				str_end--;
				break;
			}	/* fall through */
		case KEY_BACKSPACE: 
			if (!str_cur) {
				beep();
				break;
			}
			memmove( str+str_cur-1, str+str_cur, str_end-str_cur);
			str_cur--;
			str_end--;
			break;
		case CTRL('j'):		/* enter */
			done = 1;
			break;
		case KEY_LEFT: 
			if (str_cur)
				str_cur--;
			else
				beep();
			break;
		case KEY_RIGHT:
			if (str_cur < str_end)
				str_cur++;
			else
				beep();
			break;
		default:
			if (!isgraph( key) && (key != ' '))
				break;
			if (str_end == 255)
				break;
			if (str_cur != str_end) 
				memmove( str+str_cur+1,str+str_cur, str_end-str_cur);
			str[str_cur] = key;
			str_cur++;
			str_end++;
			break;
		}
		disp_start = str_cur - scrsize + 1;
		if (disp_start < 0)
			disp_start = 0;
		move( Y_MESSAGES, scr_beg-1);
		if (disp_start)
			addch( '<');
		else
			addch( ' ');
		for (i=0; i<scrsize; i++) {
			if (disp_start+i < str_end) {
				if (c)
					addch( c | A_REVERSE);
				else
					addch( str[disp_start+i] | A_REVERSE);
			} else
				addch( ' ' | A_REVERSE);
		}
		move( Y_MESSAGES, scr_beg + str_cur - disp_start);
		refresh();
	}
	msg( "");
	if (str_end == 0)
		return (NULL);
	str[str_end] = '\0';
	return (str);
}

/* ------------------------------------------------------------------------
 * select_process: Let the user select a process with the arrow-keys. */

int select_process( void)
{
	int i, c, old;
	char line[SCREEN_WIDTH+1];

	i = 0; old = 0;
	msg( "up/down, q)uit, enter=go");
	mvinnstr( Y_PROCESSES, 0, line, SCREEN_WIDTH);
	attrset( A_REVERSE); mvaddstr( Y_PROCESSES, 0, line); attrset( 0); 
	refresh();
	while ((c = xgetch( BIG_SLEEP, 1)) !=  CTRL('j')) {
		switch (c) {
		case 'j': case KEY_DOWN:
			if (i < nprocs-1) 
				old = i++;
			break;
		case 'k': case KEY_UP:
			if (i > 0)
				old = i--;
			break;
		case 'q':
			return (-1);
		default:
			old = i;
			break;
		}
		if (i != old) {
			mvinnstr( Y_PROCESSES+old, 0, line, SCREEN_WIDTH);
			mvaddstr( Y_PROCESSES+old, 0, line); 
			mvinnstr( Y_PROCESSES+i, 0, line, SCREEN_WIDTH);
			attrset( A_REVERSE); mvaddstr( Y_PROCESSES+i, 0, line); attrset( 0);
			refresh();
		}
	}
	msg( "");
	return (i);
}
	
/* ------------------------------------------------------------------------
 * let_user_kill: Let the user select a process and send a signal to it.
 * If how == KILL_NICE, a SIGHUP is sent, then a SIGTERM and if the process
 * isn't gone by now, a SIGKILL. KILL_BRUTE sends a SIGKILL right away. */
 
void let_user_kill( int how)
{
	int i;

	title( "Kill process");
	refresh();
	if ((i = select_process()) == -1)	
		return;

	switch (how) {
	case KILL_NICE:
		msg( "Sending SIGHUP...");
		if (kill( pids[i], SIGHUP)) {
			if (errno == EPERM) 
				notice( "Permission denied");
			else 
				notice( "Error!");
			return;
		} 
		refresh(); xsleep( 10);
		msg( "Sendig SIGTERM...");
		if (kill( pids[i], SIGTERM)) {
			if (errno == ESRCH) 
				notice( "Killed!");
			else
				notice( "Error!");
			return;
		}
		refresh(); xsleep( 10);
		msg( "Sendig SIGKILL...");
		if (kill( pids[i], SIGKILL)) {
			if (errno == ESRCH)
				notice( "Killed!");
			else
				notice( "Error!");
			return;
		}
		refresh(); xsleep (10);
		if (kill( pids[i], SIGKILL)) {
			if (errno == ESRCH)
				notice( "Killed");
			else
				notice( "Error!"); 
		} else
			notice( "Failed");
		break;

	case KILL_BRUTE:
		if (kill( pids[i], SIGKILL)) {
			if (errno == EPERM) 
				notice( "Permission denied");
			else
				notice( "Error!");
			return;
		}
		notice( "SIGKILL sent");
		break;
	}
}

	
/* ------------------------------------------------------------------------
 * Let the user select a process, prompt for a message and send this 
 * message to standard output of the selected process. */

void let_user_write( void)
{
	char fdname[FILENAME_MAX];
	char * ptr;
	int i;
	FILE * f;

	title( "Write to process");
	refresh();
	if ((i = select_process()) == -1)
		return;
	sprintf( fdname, "/proc/%d/fd/1", pids[i]);

	if (!(f = fopen( fdname, "w"))) {
		if (errno == EACCES)
			notice( "Permission denied");
		else
			notice( "Error!");
		return;
	}

	if (!(ptr = get_string( "msg", 0)))
		return;
	fprintf( f, "Message from %s@%s:\n\n", user, hostname);
	fputs( ptr, f);
	fprintf( f, "\a\n\n");
	fclose( f);
}


/* ------------------------------------------------------------------------
 * let_user_renice: Let the user select a process, prompt for a value and
 * renice the selected process to this value. Too high or too low values
 * are truncated. */

void let_user_renice( void)
{
	int i, level;
	char * ptr;
	
	title( "Renice process");
	refresh();
	if ((i = select_process()) == -1)
		return;
	if (!(ptr = get_string( "level", 0)))
		return;
	if (!(sscanf( ptr, "%d", &level))) {
		notice( "Illegal value");
		return;
	}
	if (level < -20)
		level = -20;
	else if (level > 20)
		level = 20;
	if (setpriority( PRIO_PROCESS, pids[i], level)) {
		if ((errno == EPERM) || (errno == EACCES)) 
			notice( "Permission denied");
		else
			notice( "Error!");
		return;
	}
	notice( "priority set");
}
	
/* ------------------------------------------------------------------------
 * xgetch: Get a character stroke from the user. 'tmout' is the time to block
 * in tenth of seconds. If 'ignoresignals' is zero, we return on a signal. 
 * If nonzero, we signals are ingnored. */

int xgetch( int tmout, int ignoresignals)
{
	fd_set rfds;
	struct timeval tv;
	int retval;
	int c;
	
	tv.tv_sec = tmout / 10;
	tv.tv_usec = (tmout % 10) * 100000;
	FD_ZERO( &rfds);
	FD_SET( STDIN_FILENO, &rfds);
	while (((retval = select( 1, &rfds, NULL, NULL, &tv)) == -1) &&
	        (ignoresignals) && (errno == EINTR));
	if (retval == 1) {
		if ((c = getch()) != ERR)
			return (c);
		else
			return (-1);
	} else
		return (-1);
}
		
/* ------------------------------------------------------------------------
 * We use select() here to sleep, because:
 * - We can't use sleep(), because we already use SIGALRM to call the
 *   update() routine.
 * - This way, we can sleep with subsecond precision.  */

void xsleep( int tmout)
{
	struct timeval tv;

	tv.tv_sec = tmout / 10;
	tv.tv_usec = (tmout % 10) * 100000;
	select( 0, NULL, NULL, NULL, &tv);
}

/* ------------------------------------------------------------------------
 * screen_setup: Draw up the screen skeleton. */

void screen_setup( void)
{
	int i;

	clear();
	attrset( A_BOLD);
	mvprintw( 1,  0, "Load:                     ");
	mvprintw( 2,  0, "CPU:                      ");
	mvprintw( 3,  0, "Mem:         Swap:        ");
	mvprintw( 4,  0, "Logins:         Real:     ");
	mvprintw( 5,  0, "XLogins:        Real:     ");

	for (i=0; (i < MAX_GROUPS) && (i < ngroups); i++)
		mvprintw( 6+i, 0, "%-12.12s              ", groups[i].name);
	for (; i<MAX_GROUPS; i++)
		mvprintw( 6+i, 0, "                          ");

	attrset( 0);
	mvprintw( 8,  0, "--------------------------");
	mvprintw( 9,  0, "                          ");
	mvprintw( 10, 0, "                          ");
	mvprintw( 11, 0, "                          ");
	mvprintw( 12, 0, "                          ");
	mvprintw( 13, 0, "                          ");
	mvprintw( 14, 0, "                          ");
	mvprintw( 15, 0, "                          ");
	mvprintw( 16, 0, "                          ");
	mvprintw( 17, 0, "                          ");
	mvprintw( 18, 0, "                          ");
	mvprintw( 19, 0, "                          ");
	mvprintw( 20, 0, "                          ");
	mvprintw( 21, 0, "                          ");
	mvprintw( 22, 0, "__________________________");
	refresh();
}


/* ------------------------------------------------------------------------
 * show_help: Show the online help. */
 
void show_help( void)
{
	title( "Hifs v%d.%d Help", HIFS_MAJOR, HIFS_MINOR);
	mvprintw( 1,  0, "  The following keys are  ");
	mvprintw( 2,  0, "    available in hifs:    ");
	mvprintw( 3,  0, "--------------------------");
	mvprintw( 4,  0, "s - Toggle sorting mode   ");
	mvprintw( 5,  0, "    (cpu/rss/vsize)       ");
	mvprintw( 6,  0, "i - Toggle info mode      ");
	mvprintw( 7,  0, "    (pid/cmd/wchan/name/  ");
	mvprintw( 8,  0, "    prio)                 ");
	mvprintw( 9,  0, "m - Toggle memory mode    ");
	mvprintw( 10, 0, "    (free/used)           ");
	mvprintw( 11, 0, "                          ");
	mvprintw( 12, 0, "k - Select and kill a proc");
	mvprintw( 13, 0, "K - Select and KILL a proc");
	mvprintw( 14, 0, "w - Write a msg to a proc ");
	mvprintw( 15, 0, "p - Set priority of a proc");
	mvprintw( 16, 0, "                          ");
	mvprintw( 17, 0, "u - Set update period     ");
#ifdef CONFIG_SU
	mvprintw( 18, 0, "r - Toggle su to root     ");
#else
	mvprintw( 18, 0, "                          ");
#endif
	mvprintw( 19, 0, "                          ");
	mvprintw( 20, 0, "CTRL-L - redraw screen    ");
	mvprintw( 21, 0, "q - quit hifs             ");
	mvprintw( 22, 0, "--------------------------");
	msg("Press any key to continue "); 
	refresh(); xgetch( BIG_SLEEP, 1);
	msg( "");
}	

/* ------------------------------------------------------------------------
 * screen_init: Initialse the screen subsystem. This function can be
 * called five times at startup. Each time, one extra dot is shown as an
 * indication of the progress made. */

int screen_init( int i)
{
	char buf[BUFSIZ];
	int row, col, j;

	if (!i) {
		gethostname( buf, BUFSIZ);
		for (j=0; j<MAX_HOSTNAME+1; j++)
			if ((buf[j] == '.') || !buf[j])
				break;
		buf[j] = '\000';
		strcpy( hostname, buf);
		strcpy( Hostname, hostname);
		Hostname[0] = toupper( Hostname[0]);

		messages = xmalloc( messages_size * sizeof (struct msg_entry));

		initscr(); noecho(); cbreak(); keypad( stdscr, TRUE);
		getmaxyx( stdscr, row, col);
		if ((row < SCREEN_HEIGHT) || (col < SCREEN_WIDTH)) {
			endwin();
			fprintf( stderr, "Hifs requires a 26x24 window\n");
			return (1);
		}
		screen_setup();
		return (0);
	} else {
		title( "Hifs Initialisation");
		strcpy( buf, "Progress: [    ]");
		for (j=0; j<i; j++)
			buf[11+j] = '.';
		msg( buf);
		refresh();
	}
	return (0);
}

/* ------------------------------------------------------------------------
 * screen_close: Clean up the resources used by the screen subsystem,
 * close the screen. */

void screen_close( void)
{
	endwin();
}

