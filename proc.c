/* vi: ts=4 sw=4
 * 
 * Hifs -- Handy Information For Sysadmins
 * Copyright (C) 1996,1997 Geert Jansen 
 */

#include "hifs.h"

/* ------------------------------------------------------------------------
 * Globals: it's allways a good thing to initialise them */

int		jiffies 		= 0; 	/* # of ticks since last update		*/
int		totaljiffies 	= 0;	/* system's total ticks				*/
int 	procs_maxi		= 0;	/* maximum index in process table	*/
int	 	procs_size		= 32;	/* initial process table size		*/
int		nlogins			= 0;	/* # of entry's in utmp				*/
int		logins_size		= 32;	/* initial login table size			*/
int		nwchans			= 0;	/* # of symbols in symbol table		*/
int		wchans_size		= 32;	/* initial entry's malloced			*/

double 	loads[3]		= { 0, 0, 0};			/* load averages	*/

struct cpu_info  		cpu;					/* cpu state info	*/
struct mem_info  		mem;					/* memory info		*/

struct utmp * 			logins 		= NULL;		/* utmp entry's		*/
struct process_info * 	procs		= NULL;		/* all processes	*/
struct wchan_entry * 	wchans		= NULL;		/* all wchan's		*/

/* ------------------------------------------------------------------------
 * Function prototypes */

int			read_procs			(void);
int			read_loads			(void);
int 		read_cpu			(void);
int			read_mem			(void);
int			read_logins			(void);
int			check_diskfree		(void);
void 		update_jiffies		(void);

const char *	strwchan		(unsigned long);

/* ------------------------------------------------------------------------
 * utmpcomp: Compare two struct utmp's. Used with qsort(). */

int utmpcomp( struct utmp * one, struct utmp * two)
{
	return (strncmp( one->ut_user, two->ut_user, UT_NAMESIZE));
}

/* ------------------------------------------------------------------------
 * read_logins: Read and sort utmp. */

int read_logins( void)
{
	struct utmp * ut;

	setutent(); nlogins = 0;
	while ((ut = getutent())) {
		if (ut->ut_type != USER_PROCESS)
			continue;
		if (nlogins == logins_size)
			logins = xrealloc( logins, (logins_size *= 2) * sizeof
					(struct utmp));
		logins[nlogins++] = *ut;
	}

	qsort( logins, nlogins, sizeof (struct utmp), 
	       (int (*)(const void *, const void *)) utmpcomp);
	return (0);
}


/* ------------------------------------------------------------------------
 * update_jiffies: Update jiffies and totaljiffies counters. Jiffies is the
 * number of ticks since the last update, totaljiffies is the system's 
 * total. */

void update_jiffies( void)
{
	FILE * statfile;
	int i;

	if (!(statfile = fopen( "/proc/uptime", "r"))) {
		queue_msg( MAX_PRIO, "/proc/uptime: %s", strerror( errno));
		return;
	}
	if (fscanf( statfile, "%d.%d", &i, &jiffies) != 2) {
		queue_msg( MAX_PRIO, "/proc/uptime: ? format");
		fclose( statfile);
		return;
	}
	fclose( statfile);
	jiffies += i*HZ;
	i = jiffies;
	jiffies -= totaljiffies;
	totaljiffies = i;
}

/* ------------------------------------------------------------------------
 * We keep all the data off the processes in the global array 'procs'.
 * This array can become big, so we keep a maximum index, the global 
 * 'procs_maxi'. There may be used records below this index, not above.
 * Further, we try to decrement this index by one every cycle. I really
 * have no idea wether this algorithm is efficient or not.
 */

int read_procs( void)
{
	char statname[FILENAME_MAX];
	char buf[BUFSIZ];
	int i, j, k, pid, found;
	unsigned long utime, stime;
	struct dirent * dentry;
	struct passwd * pwd;
	FILE * statfile;
	DIR * procdir;

	static int serial = 0;

	serial++;
	if (!(procdir = opendir( "/proc"))) {
		queue_msg( MAX_PRIO, "/proc/: %s", strerror( errno));
		return (1);
	}
	while ((dentry = readdir( procdir))) {
		if (!(pid = atoi( dentry->d_name))) 
			continue;
		found = 0;
		for (i=0; i<procs_maxi; i++) {
			if (pid == procs[i].pid) {
				found = 1;
				break;
			}
		}

		if (!found) {
			for (i=0; i<procs_maxi && procs[i].pid; i++);
			if (i == procs_maxi) {
				if (i == procs_size)
					procs = xrealloc( procs, (procs_size *= 2) * sizeof
							(struct process_info));
				i = procs_maxi++;
			}
			memset( procs+i, 0, sizeof (struct process_info));
			procs[i].pid = pid;
		}
			
		/* /proc/<pid>/stat */

		sprintf( statname, "/proc/%d/stat", pid);
		if (!(statfile = fopen( statname, "r"))) {
			queue_msg( MAX_PRIO, "%s: %s", statname, strerror( errno));
			continue;
		}
		if (fscanf( statfile, "%*d (%31[^)]) %c %*d %*d %*d %*d %*d %*u %*u" 
				"%*u %*u %*u %lu %lu %*d %*d %*d %ld %*d %*d %*u %lu %ld %*u"
				"%*u %*u %*u %*u %*u %*u %*u %*u %*u %lu %*u %*u",
		    	procs[i].comm, &procs[i].state, &utime, &stime, 
				&procs[i].priority, &procs[i].vsize, &procs[i].rss, 
				&procs[i].wchan) != 8)  {
			queue_msg( MAX_PRIO, "%s: ? format", statname);
			fclose( statfile);
			continue;
		}
		procs[i].rss *= getpagesize();
		fclose( statfile);

		procs[i].serial = serial;
		procs[i].index++;
		j = (procs[i].index &= 7);
		procs[i].times[j] = (double) (utime + stime - procs[i].jiffies) 
				/ jiffies;
		procs[i].pct_cpu = procs[i].times[j] * WEIGHT_1 + 
			procs[i].times[(j-1) & 7] * WEIGHT_2 +
			procs[i].times[(j-2) & 7] * WEIGHT_3;
		procs[i].jiffies = utime + stime;
		strnzcpy( procs[i].strwchan, strwchan( procs[i].wchan), 
				PINFO_WCHAN_SIZE);

		/* /proc/<pid>/status */

		sprintf( statname, "/proc/%d/status", pid);
		if (!(statfile = fopen( statname, "r"))) {
			queue_msg( MAX_PRIO, "%s: %s", statname, strerror( errno));
			continue;
		}
		for (k=0; k<5; k++)
			fgets( buf, BUFSIZ, statfile);
		if (sscanf( buf, "Uid: %d %d %d %d", &procs[i].uid, 
				&procs[i].euid, &procs[i].suid, &procs[i].fsuid) != 4) {
			queue_msg( MAX_PRIO, "%s: ? format", statname);
			fclose( statfile);
			continue;
		}
		fgets( buf, BUFSIZ, statfile);
		if (sscanf( buf, "Gid: %d %d %d %d", &procs[i].gid, 
				&procs[i].egid, &procs[i].sgid, &procs[i].fsgid) != 4) {
			queue_msg( MAX_PRIO, "%s: ? format", statname);
			fclose( statfile);
			continue;
		}
		fclose( statfile);
		if (!(pwd = getpwuid( procs[i].uid)))
			sprintf( procs[i].user, "%d", procs[i].uid);
		else
			strnzcpy( procs[i].user, pwd->pw_name, PINFO_USER_SIZE);

		/* /proc/<pid>/cmdline */

		sprintf( statname, "/proc/%d/cmdline", pid);
		if (!(statfile = fopen( statname, "r"))) {
			queue_msg( MAX_PRIO, "%s: %s", statname, strerror( errno));
			continue;
		}
		j = fread( procs[i].cmdline, sizeof (char), PINFO_CMDLINE_SIZE-1, 
				statfile);
		for (k=0; k<j; k++)
			if (!procs[i].cmdline[k])
				procs[i].cmdline[k] = ' ';
		procs[i].cmdline[j] = '\000';
		fclose( statfile);
	}

	closedir( procdir);

	/* Remove dead processes from the process table */

	for (i=0; i<procs_maxi; i++)
		if (procs[i].serial != serial)
			procs[i].pid = 0;

	/* Try to decrement max counter by one ... */

	if (!procs[procs_maxi-1].pid)
		procs_maxi--;

	return (0);
}

/* ------------------------------------------------------------------------
 * read_loads: Read the system's load average from /proc/loadavg into 
 * the global array `loads'.
 */

int read_loads( void)
{
	FILE * statfile;

	if (!(statfile = fopen( "/proc/loadavg", "r"))) {
		queue_msg( MAX_PRIO, "/proc/loadavg: %s", strerror( errno));
		return (1);
	}
	if (fscanf( statfile, "%lf %lf %lf", loads, loads+1, loads+2) != 3) {
		queue_msg( MAX_PRIO, "/proc/loadavg: ? format");
		fclose( statfile);
		return (1);
	}
	fclose( statfile);
	return (0);
}

/* ------------------------------------------------------------------------
 * check_diskfree: Check mounted filesystems for free diskspace. Give a
 * message when it drops below `min_diskfree'. Only check selected filesystem 
 * that are meant to be "native" and that are not mounted ro. Native means
 * that you actually use them, not just to access you DOS games. */

int check_diskfree( void)
{
	char buf[BUFSIZ], device[FILENAME_MAX]; 
	char mntpoint[FILENAME_MAX], type[1024], rw[1024];
	char * fstypes[8] = { "ext2", "nfs", "umsdos", NULL };
	int i, full;
	struct statfs f;
	FILE * statfile;
	
	/* /proc/mounts is available since kernel version 1.3.64 */

	if (version_code < 1003064)
		return (0);

	if (!(statfile = fopen( "/proc/mounts", "r"))) {
		queue_msg( MAX_PRIO, "/proc/mounts: %s", strerror( errno));	
		return (1);
	}
	full = 0;
	while (fgets( buf, BUFSIZ, statfile)) {
		if (sscanf( buf, "%s %s %1023s %1023s", device, mntpoint, type, 
				rw) != 4)
			continue;
		for (i=0; fstypes[i]; i++)
			if (!strcmp( fstypes[i], type))
				break;
		if (!fstypes[i])
			continue;
		if (!strncmp( rw, "ro", 2))
			continue;
		statfs( mntpoint, &f);
		if (f.f_bavail < (min_diskfree/f.f_bsize)) {
			queue_msg( MED_PRIO, "%.18s is FULL!!", mntpoint);
			full = 1;
		}
	}
	fclose( statfile);
	if (!full) 
		queue_msg( MIN_PRIO, "No filesystems are full");

	return (0);
}

/* ------------------------------------------------------------------------
 * read_cpu: Read the cpu states. We use the same decay here as with the
 * individual processes. */

int read_cpu( void)
{
	FILE * statfile;
	unsigned int user, nice, system, idle, i;

	if (!(statfile = fopen( "/proc/stat", "r"))) {
		queue_msg( MAX_PRIO, "/proc/stat: %s", strerror( errno));
		return (1);
	}
	if (fscanf( statfile, "cpu %u %u %u %u", &user, &nice, &system, 
			&idle) != 4) {
		queue_msg( MAX_PRIO, "/proc/stat: ? format");
		fclose( statfile);
		return (1);
	}
	cpu.index++;
	i = (cpu.index &= 7);

	cpu.idle[i] = (double) (idle - cpu.idlejiffies) / jiffies;
	cpu.pct_idle = cpu.idle[i] * WEIGHT_1 +
			     cpu.idle[(i-1) & 7] * WEIGHT_2 +
			     cpu.idle[(i-2) & 7] * WEIGHT_3;
	cpu.idlejiffies = idle;

	cpu.user[i] = (double) (user - cpu.userjiffies) / jiffies;
	cpu.pct_user = cpu.user[i] * WEIGHT_1 +
			     cpu.user[(i-1) & 7] * WEIGHT_2 +
			     cpu.user[(i-2) & 7] * WEIGHT_3;
	cpu.userjiffies = user;

	cpu.nice[i] = (double) (nice - cpu.nicejiffies) / jiffies;
	cpu.pct_nice = cpu.nice[i] * WEIGHT_1 +
			     cpu.nice[(i-1) & 7] * WEIGHT_2 +
			     cpu.nice[(i-2) & 7] * WEIGHT_3;
	cpu.nicejiffies = nice;

	cpu.system[i] = (double) (system - cpu.systemjiffies) / jiffies;
	cpu.pct_system = cpu.system[i] * WEIGHT_1 +
			     cpu.system[(i-1) & 7] * WEIGHT_2 +
			     cpu.system[(i-2) & 7] * WEIGHT_3;
	cpu.systemjiffies = system;

	fclose( statfile);
	return (0);
}

/* ------------------------------------------------------------------------
 * read_mem: Read the memory statistics. */

int read_mem( void)
{
	char buf[BUFSIZ];
	FILE * statfile;
	char fmt[32];
	int i, format;
	struct {
		char * str;
		unsigned long * i;
	} lines[] = {
		{ "MemTotal", &mem.total},
		{ "MemFree", &mem.free},
		{ "MemShared", &mem.shared},
		{ "Buffers", &mem.buffers},
		{ "Cached", &mem.cached},
		{ "SwapTotal", &mem.swaptotal},
		{ "SwapFree", &mem.swapfree},
		{ NULL, NULL}
	};

	if (!(statfile = fopen( "/proc/meminfo", "r"))) {
		queue_msg( MAX_PRIO, "/proc/meminfo: %s", strerror( errno));
		return (1);
	}

	/* The format of /proc/meminfo changed in 2.1.41 but changed back
	 * in 2.1.52 ... We auto detect it here, so the kernel guys can 
	 * change it back and forth if they want. */

	fgets( buf, BUFSIZ, statfile);
	if (!strncmp( buf, lines[0].str, strlen( lines[0].str)))
		format = 1;
	else
		format = 0;
	fseek( statfile, 0, SEEK_SET);

	if (!format) {
		fgets( buf, BUFSIZ, statfile);
		fgets( buf, BUFSIZ, statfile);
		if (sscanf( buf, "Mem: %lu %lu %lu %lu %lu %lu", &mem.total, &mem.used, 
				&mem.free, &mem.shared, &mem.buffers, &mem.cached) != 6) {
			queue_msg( MAX_PRIO, "/proc/meminfo: ? format");
			fclose( statfile);
			return (1);
		}
		fgets( buf, BUFSIZ, statfile);
		if (sscanf( buf, "Swap: %lu %lu %lu", &mem.swaptotal, &mem.swapused, 
				&mem.swapfree) != 3) {
			queue_msg( MAX_PRIO, "/proc/meminfo: ? format");
			fclose( statfile);
			return (1);
		}
		fclose( statfile);
	} else {
		for (i=0; lines[i].str; i++) {
			sprintf( fmt, "%s: %%d", lines[i].str);	/* Make format string	*/
			fgets( buf, BUFSIZ, statfile);
			if (!sscanf( buf, fmt, lines[i].i)) {
				queue_msg( MAX_PRIO, "/proc/meminfo: ? format");
				fclose( statfile);
				return (1);
			}
			*lines[i].i <<= 10;						/* Convert to bytes		*/
		}
		
		mem.used = mem.total - mem.free;
		mem.swapused = mem.swaptotal - mem.swapfree;
	}
		
	return (0);
}

/* ------------------------------------------------------------------------
 * proc_update: Update all statistics. Clear all messages.  This routine is 
 * a signal handler for SIGALRM. */

void proc_update( int sig)
{
	static int skip = 0;
	sigset_t set;
	
	if (skip) {
		skip--;
		return;
	}
	update_jiffies();

	read_procs();
	read_cpu();
	read_loads();
	read_mem();
	read_logins();
	check_diskfree();

	/* We must check if the update takes more time than the update period.
	 * If this is the case, the main program loop does not run because
	 * after this handler exits, it will be called right away.
	 * I do a simple test here to see if this is the case. It is not
	 * water-tight (i.e. a signal can arrive after the sigpending() call)
	 * and it can actually miss a signal or two, but as the time that is
	 * needed to update the data is large as compared to the time it takes
	 * to draw the screen, it is highly unlikely that we keep blocked.
	 * A second reason not to put magic in here is that if the delay is
	 * such that updates are following each other up, hifs is cpu bound
	 * and will eat up all your cpu cycles. Not very usefull for a 
	 * diagnostic program ... */

	if (sigpending( &set)) {
		queue_msg( MAX_PRIO, "sigpending: %s", strerror( errno)) ;
		return;
	}
	if (sigismember( &set, SIGALRM)) {
		queue_msg( MAX_PRIO, "Delay too short!");
		skip++;
	}
}

/* ------------------------------------------------------------------------
 * strwchan: Get the symbol name of wchan. We use bisection to find the
 * appropriate entry in the array. */

const char * strwchan( unsigned long wchan)
{
	int a, b, t;
	unsigned long tval;
	static char buf[32];
	
	/* If no string representations of wchans are loaded, return numeric.
	 * Also, if wchan is zero, this means not in kernel space and we
	 * return a "0". */

	if (!(nwchans && wchan)) {
		sprintf( buf, "%lx", wchan);
		return (buf);
	}

	a = 0; b = nwchans;
	while (b-a != 1) {
		t = (a + b) / 2;
		tval = wchans[t].wchan;
		if (wchan >= tval)
			a = t;
		else
			b = t;
	}
	return (wchans[a].strrep);
}

/* ------------------------------------------------------------------------
 * Load the kernel symbol table in the array 'wchans'. */

int load_wchans( void)
{
	char buf[BUFSIZ];
	int i = 0, j, line;
	FILE * fin;

	char * def_mapfiles[] = {
		"/boot/System.map-%s",
		"/boot/System.map",
		"/lib/modules/%s/System.map",
		"/usr/src/linux/System.map",
		NULL
	};
	char * mapfiles[16];
	
	/* Create a list of all mapfiles to try */

	if (mapfile[0])
		mapfiles[i++] = mapfile;
	for (j=0; def_mapfiles[j]; j++) {
		if (strchr( def_mapfiles[j], '%')) {
			mapfiles[i] = xmalloc( 128);
			sprintf( mapfiles[i++], def_mapfiles[j], version_string);
		} else
			mapfiles[i++] = def_mapfiles[j];
	}
	mapfiles[i++] = NULL;

	/* and try them ... */

	for (i=0; mapfiles[i] && !(fin = fopen( mapfiles[i], "r")); i++);
	if (!mapfiles[i])
		return (1);
	
	line = 0;
	while (fgets( buf, BUFSIZ, fin)) {
		line++;
		if (wchans_size == nwchans)  
			wchans = xrealloc( wchans, (wchans_size *= 2) * 
                          sizeof (struct wchan_entry));
		if (sscanf( buf, "%lx %*c %31s", &wchans[nwchans].wchan, 
                    wchans[nwchans].strrep) != 2) {
			fprintf( stderr, "%s, line %d: parse error\n", mapfile, line);
			fclose( fin); nwchans = 0;
			return (1);
		}
		nwchans++;
	}
	fclose( fin);
	return (0);
}

/* ------------------------------------------------------------------------
 * proc_init: Initialise various things of the proc subsystem. */

int proc_init( void)
{
	if (access( "/proc/version", R_OK)) {
		fprintf( stderr, "Proc filesystem is not mounted on /proc\n");
		return (1);
	}

	logins = xmalloc( logins_size * sizeof (struct utmp));
	procs = xmalloc( procs_size * sizeof (struct process_info));
	wchans = xmalloc( wchans_size * sizeof (struct wchan_entry));

	if (load_wchans())
		warned = 1;
	
	utmpname( _PATH_UTMP);

	return (0);
}
/* ------------------------------------------------------------------------
 * proc_close: Clean up process subsystem. Currently a no-op */

void proc_close( void)
{
	return;
}

