/* vi: ts=4 sw=4
 * 
 * Hifs -- Handy Information For Sysadmins
 * Copyright (C) 1996,1997 Geert Jansen 
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The author can be contacted via email as <geertj@stack.nl>
 */

#include "hifs.h"

/* ------------------------------------------------------------------------
 * Globals. */

char *			tty 		= NULL;
struct stat		ttybuf;
int				warned		= 0;
int				rootflag	= 0;
char *			user		= NULL;

struct group * 	groups		= NULL;
int 			ngroups		= 0;
int				mlgroups	= 0;

char 			config_file[FILENAME_MAX];
int				version_code;
char *			version_string;

/* Defaults */

double			delay		= 5;
int				sort		= SORT_CPU;
int				memory		= MEM_FREE;
int				info		= INFO_NAME;
char *			mapfile		= "";

int				min_diskfree	= 1000000;
int				debug		= 0;

/* Tables for string representations of sort/info/mem modes */

struct mode sortmodes[] = {
	{ "By CPU", "CPU" },
	{ "By RSS", "RSS" },
	{ "By Vsize", "VSZ" }
};

struct mode infomodes[] = {
	{ "Process ID", "PID" },
	{ "Command Line", "CMD" },
	{ "Wchan", "WCH" },
	{ "Username", "NAM" },
	{ "Priority", "PRI" }
};

struct mode memmodes[] = {
	{ "Free (Kb)", "FRE" },
	{ "Used (Kb)", "USD" }
};


/* ------------------------------------------------------------------------
 * Prototypes not in hifs.h. */

void 		gracefull_exit	(int);
int			cfgfile			(void);
void		set_update		(double);
void		print_banner	(void);
void		print_help		(void);

/* ------------------------------------------------------------------------
 * cfgfile: Parse the configfile */

int cfgfile( void)
{
	char fname[FILENAME_MAX];
	int ret;
	struct passwd * pwd;

	extern FILE * yyin;
	extern int yyparse( void);

	if (!(pwd = getpwuid( getuid()))) {
		perror( "getpwuid()");
		return (1);
	}
	sprintf( fname, "%s/.hifsrc", pwd->pw_dir);

	/* If user does not have a .hifsrc, return */

	if (access( fname, F_OK))
		return (0);

	if (!(yyin = fopen( fname, "r"))) {
		perror( fname);
		return (1);
	}
	ret = yyparse();
	fclose( yyin);

	return (ret);
}

/* ------------------------------------------------------------------------
 * set_update: Set the update frequency. */

void set_update( double update)
{
	struct itimerval it;

	it.it_interval.tv_sec = update;
	it.it_interval.tv_usec = (update - floor( update)) * 1e6;
	it.it_value.tv_sec = update;
	it.it_value.tv_usec = (update - floor( update)) * 1e6;
	setitimer( ITIMER_REAL, &it, NULL);
}
	
/* ------------------------------------------------------------------------
 * gracefull_exit: This is the sig-go-away handler. */

void gracefull_exit( int sig)
{	
	proc_close();
	screen_close();
	printf( "\nExiting...(signal %d)\n", sig );
	chmod( tty, ttybuf.st_mode);
	_exit( 1);
}
	
/* ------------------------------------------------------------------------
 * print_banner: Show version information. */
 
void print_banner( void)
{
	printf( "Hifs %d.%d (%s@%s) (%s) %s\n", HIFS_MAJOR, HIFS_MINOR,
	        COMPILE_BY, COMPILE_HOST, COMPILE_CC, COMPILE_DATE);
	return;
}
	
/* ------------------------------------------------------------------------
 * print_help: Show some help about command line options. */

void print_help( void)
{
	printf( "Usage: hifs [OPTION]...\n\n");
	printf( "  -v, --version        show version information\n");
	printf( "  -h, --help           show this help\n");
	printf( "  -d, --debug          set debug mode\n");
	printf( "\n");
	return;
}

/* ------------------------------------------------------------------------
 * main: This is hifs. */

int main( int argc, char ** argv)
{
	char buf[BUFSIZ];
	char c, * ptr;
	int i, done, optindex; 
	int major, minor, patchlevel;
	struct sigaction sa;
	struct termios tioold, tionew;
	struct passwd * pwd;
	struct utsname ut;
	sigset_t sigset;

	struct option opts[] = {
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ "debug", 0, 0, 'd'}
	};

#ifdef CONFIG_SU
	char * p, * pp, rootpass[32];
	struct rlimit rlim;

#ifdef CONFIG_SHADOW
	struct spwd * sp;
#endif

	/* Set core dump limit to 0 to prevent coredump of setuid binary.
	 * This may leave the encrypted root password visible in the core
	 * image. */

	if (getrlimit( RLIMIT_CORE, &rlim)) {
		perror( "getrlimit()");
		exit( 1);
	}
	rlim.rlim_cur = 0;
	if (setrlimit( RLIMIT_CORE, &rlim)) {
		perror( "setrlimit()");
		exit( 1);
	}

	/* Open the system log as `hifs' */

	openlog( "hifs", LOG_PID, LOG_USER);
	if (!(pwd = getpwuid( getuid()))) {
		syslog( LOG_WARNING, "started by uid %d not in password"
				" file!", getuid());
		exit( 1);
	}

	user = xmalloc( strlen( pwd->pw_name)+1);
	strcpy( user, pwd->pw_name);
	syslog( LOG_INFO, "started by %s (uid %d)", user, getuid());

	if (!(pwd = getpwuid( 0))) {
		perror( "getpwuid()");
		exit( 1);
	}
	strnzcpy( rootpass, pwd->pw_passwd, 32);

#ifdef CONFIG_SHADOW
	if ((sp = getspnam( pwd->pw_name)))
		strnzcpy( rootpass, sp->sp_pwdp, 32);
#endif

	/* Drop priviliges */

	if (seteuid( getuid())) {
		perror( "seteuid()");
		exit( 1);
	}

#else	/* ! CONFIG_SU */

	if (!(pwd = getpwuid( getuid()))) {
		perror( "getpwuid()");
		exit( 1);
	}
	user = xmalloc( strlen( pwd->pw_name)+1);
	strcpy( user, pwd->pw_name);

#endif  /* CONFIG_SU */

	/* Parse command-line arguments */
	
	while ((c = getopt_long( argc, argv, "vhd", opts, &optindex)) != EOF) {
		switch (c) {
		case 'v':
			print_banner();
			exit( 0);
		case 'h':
			print_help();
			exit( 0);
		case 'd':
			debug = 1;
			break;
		case '?':
			printf( "Try `hifs --help' for more information.\n");
			exit( 1);
		}
	}
	
	/* Get the kernel version */

	if (uname( &ut)) {
		perror( "uname()");
		exit( 1);
	}
	if (sscanf( ut.release, "%d.%d.%d", &major, &minor, &patchlevel) != 3) {
		fprintf( stderr, "kernel version: %s: unknown format\n", ut.release);
		exit( 1);
	}
	version_code = 1000000 * major + 1000 * minor + patchlevel;
	version_string = xmalloc( sizeof( ut.release) + 1);
	strcpy( version_string, ut.release);

	/* Parse the configfile */

	if (cfgfile()) {
		fprintf( stderr, "Failed to read the configfile\n");
		exit( 1);
	}
	
	if (proc_init()) {
		fprintf( stderr, "Initialisation of proc subsystem failed\n");
		fprintf( stderr, "Exiting...");
		exit( 1);
	}

	/* We make our tty mode 0600 to prevent talk's and write's to this
	 * window. */

	if (!(tty = ttyname(1))) {
		perror( "ttyname()");
		exit( 1);
	}	
	if (stat( tty, &ttybuf)) {
		perror( "stat()");
		warned = 1;
	}
	if (chmod( tty, 0600)) {
		perror( "chmod()");
		warned = 1;
	}

	/* If we printed a warning, wait for the user here if in debug mode. */

	if (warned && debug) {

		/* Save terminal attributes and go to cbreak mode. Then, wait
		 * for the user to press a key. */

		if (tcgetattr( STDIN_FILENO, &tioold)) {
			perror( "tcgetattr()");
			exit (1);
		}
		tionew = tioold;
		tionew.c_lflag &= ~(ICANON | ECHO);
		tionew.c_cc[VTIME] = 0;
		tionew.c_cc[VMIN] = 1;
		if (tcsetattr( STDIN_FILENO, TCSANOW, &tionew)) {
			perror( "tcsetattr()");
			exit (1);
		}
		printf( "\npress ANY key to continue");
		fflush( stdout);
		read( STDIN_FILENO, buf, 1);

		/* Reset terminal parameters */

		tcflush( STDIN_FILENO, TCIFLUSH);	/* discard more input */
		if (tcsetattr( STDIN_FILENO, TCSANOW, &tioold)) 
			perror( "tcsetattr()");
	}
	
	/* Initialise the screen */
	
	if (screen_init( 0)) {
		fprintf( stderr, "Initialisation of screen subsystem failed\n");
		fprintf( stderr, "Exiting...");
		exit( 1);
	}

	/* Initialise the cpu usage histories */

	for (i=1; i<5; i++) {
		proc_update( 0);
		screen_init( i);
		xsleep( 10);
	}
		
	/* Set up signal handlers.	*/

	sa.sa_handler = gracefull_exit;
	sigemptyset( &sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_restorer = NULL;
	sigaction( SIGHUP, &sa, NULL);
	sigaction( SIGINT, &sa, NULL);
	sigaction( SIGQUIT, &sa, NULL);
	sigaction( SIGTERM, &sa, NULL);
	sa.sa_handler = proc_update;
	sigaction( SIGALRM, &sa, NULL);

	/* Set updating frequency */

	set_update( delay);

	/* Create a signal set to block SIGALRM later on */

	sigemptyset( &sigset);
	sigaddset( &sigset, SIGALRM);

	/* Main program loop		*/

	done = 0;
	while (!done) {

		/* We block SIGALRM when screen_update() is called to prevent a data
		 * update while reading this data. */

		sigprocmask( SIG_BLOCK, &sigset, NULL);
		screen_update();
		sigprocmask( SIG_UNBLOCK, &sigset, NULL);

		switch (xgetch( BIG_SLEEP, 0)) {
			case 's': case ' ':
				sort++;
				if (sort > SORT_LAST)
					sort = 0;
				queue_msg( MIN_PRIO, "Sort mode: %s", sortmodes[sort].l);
				break;
			case 'i':
				info++;
				if (info > INFO_LAST)
					info = 0;
				queue_msg( MIN_PRIO, "Info mode: %s", infomodes[info].l);
				break;
			case 'm':
				memory++;
				if (memory > MEM_LAST)
					memory = 0;
				queue_msg( MIN_PRIO, "Mem mode: %s", memmodes[memory].l);
				break;
			case 'k':
				let_user_kill( KILL_NICE);
				break;
			case 'K':
				let_user_kill( KILL_BRUTE);
				break;
			case 'w':
				let_user_write();
				break;
			case 'p':
				let_user_renice();
				break;
			case 'u':
				ptr = get_string( "Update Period", 0);
				if (!ptr)
					break;
				if (sscanf( ptr, "%lf", &delay)) {
					set_update( delay);
					notice( "Update now %.2f s", delay);
				} else
					notice( "Illegal update");
				break;
			case 'h': case '?':
				show_help();	/* Fall tru */
			case CTRL('l'):
				screen_setup();
				break;

#ifdef CONFIG_SU
			case 'r':
				if (!geteuid()) {
					if (seteuid( getuid())) 
						notice( "Failed to drop privileges");
					else {
						syslog( LOG_INFO, "Priviliges dropped");
						notice( "Priviliges dropped");
						rootflag = 0;
					}
					break;
				}
				p = get_string( "pass", '*');
				if (!p)
					break;
				pp = crypt( p, rootpass);
				memset( p, 0, strlen( p));
				if (strcmp( pp, rootpass)) {
					syslog( LOG_WARNING, "Bad SU to root by %s (uid %d)",
							user, getuid());
					notice( "Illegal password!");
					break;
				}
				if (seteuid( 0)) 
					notice( "Failed to set priviliges");
				else {
					syslog( LOG_INFO, "SU to root by %s (uid %d)", 
							user, getuid());
					notice( "Hifs now running as root");
					rootflag = 1;
				}
				break;
#endif 	/* CONFIG_SU */

			case 'q':
				done = 1;
			default:
				break;
		}
	}
	
	proc_close();
	screen_close();
	chmod( tty, ttybuf.st_mode);

	return (0);
}

