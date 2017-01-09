/*
	pt.c   version 3.9   December 19, 1989

	Eric Lechner  (lechner@ucscb.ucsc.edu)

	a general purpose N-question (purity) test.
	data file format given in pt.h
	tailor definitions to your needs in pt.h
*/

#define VERSION "3.9" 			/* current test version # */
#ifdef LINUX
# define SYSV
# include <unistd.h>
# include <stdlib.h>
#endif

#include "pt.h"
#include <ctype.h>
#include <stdio.h>
#ifdef SYSV
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>  /* SYSV now uses termios instead of termio POSIX compliant */
#include <string.h>
#else
#include <sgtty.h>
#include <strings.h>
#endif /*SYSV*/
#include <sys/ioctl.h>
#include <sys/file.h>
#include <signal.h>
#include <pwd.h>

#include <curses.h>

#define SKIP 255
#define TRUE 1
#define FALSE 0
#define DO_ECHO TRUE
#define NO_ECHO FALSE
#define OK 10
#define FILE_ERR -1
#define QUIT -2
#define EXIT -3
#define BACKUP 5
#define LOOP -10

struct quest {
	int num,		/* # of question		*/
	    answer;		/* answer to current question	*/
	long fpos;		/* position in file for fseek	*/
	struct quest *last;	/* pointer to last question	*/
};

static int	question(void),
	get_response(void),
	getresp(void),
	subject(void),
	print(int, int),
	more(void),
	redraw(void);

static void	explain(void),
	quit(int),
#ifdef SIGTSTP
	ctrlz(int),
	ctrlzret(int),
#endif
	set_mode(void),
	clr_mode(void),
	freemem(void),
	printscore(void),
	sizewindow(void);

#ifndef PW
struct passwd *getpw();
#endif

static FILE *fp;

static struct quest *theq, *nextq, *the_s, *next_s;
static int num_ch, num_ln, kill_sub, no_ans, zoom, obfus, fast, derange, rot13;

static short tty_flags;
#ifdef SYSV
static unsigned char baz;
#endif

static unsigned short rows, cols;

static char *testtype;
#ifdef LOGFILE
static int logme;
#endif

int main(int argc, char *argv[])
{
	int err, ch;	/* var to store error reports */
	char path[256], *ch1;

	(void) signal(SIGINT,quit);
#ifdef SIGTSTP
	(void) signal(SIGTSTP,ctrlz);
	(void) signal(SIGCONT,ctrlzret);
#endif

#ifdef GID
	(void) setgid(GID);
#endif
	srand (getpid());	/* seed r.n.g. for derange mode */
				/* it's only derange, why not use the pid? */

	err = 0;
	*path = '\0';
	testtype = NULL;
	ch = FALSE;

	while (ch == FALSE) {
		if (++err >= argc) {
			(void) sprintf(path,"%s/%s",LIBDIR,"intro");
			ch = TRUE;
		} else {
			switch (*argv[err]) {
				case '-' :
					break;
				default :
					(void) snprintf(path, sizeof(path),"%s/%s",
							LIBDIR, argv[err]);
					ch = TRUE;
					break;
			}
		}
	}

	fp = fopen(path,"r");
	if (fp == NULL) {
		(void) snprintf(path, sizeof (path),"%s",argv[err]);
		fp = fopen(path,"r");
	}

	if (fp == NULL) {
#ifndef SYSV
		if ((ch1 = rindex(argv[0],'/'))) ch1++;
#else
		if ((ch1 = strrchr(argv[0],'/'))) ch1++;
#endif
		else ch1 = argv[0];
#ifndef SYSV
		if ((testtype = rindex(path,'/'))) testtype++;
#else
		if ((testtype = strrchr(path,'/'))) testtype++;
#endif
		else testtype = path;
		
		fprintf(stderr,"%s: error opening datafile '%s'.\n",
			ch1,testtype);
		exit(-1);
	}
	rewind (fp);

	if (testtype == NULL) {
#ifndef SYSV
		if ((testtype = rindex(path,'/'))) testtype++;
#else
		if ((testtype = strrchr(path,'/'))) testtype++;
#endif
		else testtype = path;
	}

#ifdef LOGFILE

	if (!strcmp(testtype,"intro") ||
			!strcmp(testtype,"scores") ||
			!strcmp(testtype,"format") ||
			!strcmp(testtype,"help") ||
			!strcmp(testtype,"list"))
		logme = FALSE;
	else logme = TRUE;
#endif

	theq = (struct quest *) malloc (sizeof(struct quest));
	the_s = (struct quest *) malloc (sizeof(struct quest));
	if ((theq == NULL) || (the_s == NULL)) {
		fprintf(stderr,"Memory allocation error.\n");
		(void) fclose(fp);
		exit(-1);
	}

#ifdef MASK
	(void) strncpy (argv[0],MASK,strlen(argv[0]));
#endif

	theq->num = 1;		/* setup initial question pointer info */
	theq->last = NULL;
	theq->answer = FALSE;
	the_s->num = 1;		/* setup initial subject pointer info */
	the_s->last = NULL;
	no_ans = zoom = fast = derange = rot13 = FALSE;	/* set default flags */
	obfus = TRUE;


	if (argc > 1) {		/* read command line options, set flags */
		for (err=1; err<argc; err++) {
			if ((*argv[err] == '-') || (*argv[err] == '+')) {
				while (*++argv[err]) 
				switch (*argv[err]) {
					case 'p' :
						no_ans = TRUE;
						break;
					case 'z' :
						zoom = TRUE;
						break;
#ifdef LOGFILE
					case 'l' :
						logme = FALSE;
						break;
#endif
					case 'a' :
						obfus = FALSE;
						break;
					case 'f' :
						fast = TRUE;
						break;
					case 'd' :
						derange = TRUE;
						break;
					case 'r' :
						rot13 = TRUE;
						break;
					default : break;
				}
			}
		}
	}
		
	set_mode();		/* set no-echo and cbreak modes	*/
	sizewindow();		/* get window size for "more" prompts */
	num_ch = num_ln = 0;
	kill_sub = FALSE;

#ifdef LOGFILE
	if (strcmp(testtype,"scores"))
#endif
		err = OK;
#ifdef LOGFILE
	else {
		print(DO_ECHO,EOF);
		err = EXIT;
	}
#endif

	while (err == OK) {	/* do all the test stuff... */
		ch = fgetc(fp);
		switch (ch) {
			case EOF :
				err = EXIT;
				break;
			case TEXT :
			        fprintf(stdout, "\033[31;1m");
				if (fast) err = print(NO_ECHO,ENDTEXT);
				else {
					err = print(DO_ECHO,ENDTEXT);
					(void) fputc('\n',stdout);
				}
				fprintf(stdout, "\033[0m");
				break;
			case SUBJECT :
				err = subject();
				break;
			case QUESTION :
				err = question();
				break;
			case PRINTSCORE :
				printscore();		/* print the score */
				if (!fast) err = print(DO_ECHO,ENDSCORE);
				else err = print(NO_ECHO,ENDSCORE);
				break;
			default :
				/*fputc(ch,stdout);*/
				break;
		}
	}
	(void) fclose (fp);
	(void) fflush(stdout);
	clr_mode();
	switch (err) {
		case FILE_ERR :
			fprintf(stderr,"\nFile access error.\n");
			freemem();
			exit (-1);
			break;
		case QUIT :
			printscore();
			freemem();
			fprintf (stdout,"\nseeya later, alligator.\n");
			exit(0);
			break;
		case EXIT :
			fprintf (stdout,"bye.\n");
			freemem();
			exit(0);
			break;
		default :
			break;
	}
	return 0;
}

int question()
{
	int err;

	theq->fpos = ftell(fp) - 1;

	fprintf(stdout, "\033[31;1;3m");
	if (kill_sub == FALSE) {
		fprintf(stdout,"%3d. ",theq->num);
		err = print(DO_ECHO,ENDQUEST);
		(void) fputc('\n',stdout);
	} else err = print(NO_ECHO,ENDQUEST);
	fprintf(stdout, "\033[0m");

	if (err == OK) {
		if (kill_sub || no_ans) err = SKIP;
		else err = get_response();

		switch (err) {
			case EXIT :
			case QUIT :
				break;
			case BACKUP :
				if (theq->last != NULL) {
					nextq = theq;
					theq = nextq->last;
					free((char *) nextq);
					fprintf(stdout,"\033[35;1mYour old answer was: \033[0m");
					if (theq->answer == TRUE)
						fprintf(stdout,"\033[32;1myes.\n\n\033[0m");
					else if (theq->answer == FALSE)
						fprintf(stdout,"\033[32;1mno.\n\n\033[0m");
					else fprintf(stdout,"\033[33;1mskipped.\n\n\033[0m");
					if (the_s->last != NULL) {
						next_s = the_s->last;
						if (next_s->fpos > theq->fpos) {
							free((char *) the_s);
							the_s = next_s;
						}
					}
				} else fprintf(stderr,"\033[33;7mSilly, you can't back up past the beginning of the test...\n\033[0m\n");
				err = fseek(fp,theq->fpos,0);
				if (err == EOF) err = FILE_ERR;
				else err = OK;
				break;
			default :
				theq->answer = err;
				err = OK;
				nextq = (struct quest *) malloc (sizeof(struct quest));
				if (nextq == NULL) {
					fprintf(stderr,"Memory allocation error.\n");
			 		err = EXIT;
				} else {
					nextq->num = theq->num + 1;
					nextq->last = theq;
					nextq->answer = FALSE;
					theq = nextq;
				}
				break;
		}
	}
	if (no_ans == FALSE) num_ch = num_ln = 0;
	return(err);
}

int get_response()
{
	int ch, done = FALSE;

	while (done == FALSE) {
#ifndef LOGFILE
		fprintf(stdout,"your answer? [ynbakdsrq?] : ");
#else
		fprintf(stdout,"your answer? [ynbakdlsrq?] : ");
#endif

		ch = getresp();

		switch (ch) {
			case 'y' :
			case 'Y' :
			case ' ' :
			case '\n' :
				if (obfus)
					fprintf(stdout,"%s\n\n",YES);
				else fprintf(stdout,"yes\n\n");
				ch = TRUE;
				done = TRUE;
				break;
			case 'n' :
			case 'N' :
				if (obfus)
					fprintf(stdout,"%s\n\n",NO);
				else fprintf(stdout,"no\n\n");
				ch = FALSE;
				done = TRUE;
				break;
			case 'b' :
			case 'B' :
			case '-' :
			case 'c' :
			case 'C' :
				fprintf(stdout,"backup\n\n");
				ch = BACKUP;
				done = TRUE;
				break;
			case 'x' :
			case 'X' :
			case 'q' :
			case 'Q' :
				fprintf(stdout,"quit\n");
				ch = QUIT;
				done = TRUE;
				break;
			case 's' :
			case 'S' :
				fprintf(stdout,"print score\n");
				printscore();
				ch = 's';
			case 'r' :
			case 'R' :
				if (ch != 's') fprintf(stdout,"redraw\n");
				(void) fputc('\n',stdout);
				ch = redraw();
				if (ch == FILE_ERR) {
					done = TRUE;
					ch = EXIT;
				}
				break;
			case 'h' :
			case 'H' :
			case '?' :
				fprintf(stdout,"help\n\n");
				explain();
				break;
			case 'k' :
			case 'K' :
				fprintf(stdout,"kill\n\n");
				kill_sub = TRUE;
				ch = SKIP;
				done = TRUE;
				break;
#ifdef LOGFILE
			case 'l' :
			case 'L' :
				fprintf (stdout,"logscore is now ");
				if (logme) {
					logme = FALSE;
					fprintf (stdout,"off\n");
				} else {
					logme = TRUE;
					fprintf(stdout,"on\n");
				}
				break;
#endif
			case 'd' :
			case 'D' :
				fprintf (stdout,"derange is now ");
				if (derange) {
					derange = FALSE;
					fprintf (stdout,"off\n");
				} else {
					derange = TRUE;
					fprintf(stdout,"on\n");
				}
				break;
			case 'a' :
			case 'A' :
				fprintf (stdout,"answers are now ");
				if (obfus) {
					obfus = FALSE;
					fprintf(stdout,"real\n");
				} else {
					obfus = TRUE;
					fprintf (stdout,"obfuscated\n");
				}
				break;
			default :
				fprintf(stdout,"huh?\n");
				break;
		}
	}
	return (ch);
}

void explain()
{
	fprintf(stdout,"the purity test: version %s  by Eric Lechner\n\n",
								VERSION);
	fprintf(stdout,"y - answer 'yes' to current question.\n");
	fprintf(stdout,"n - answer 'no' to current question.\n");
	fprintf(stdout,"b - backup one question.\n");
	fprintf(stdout,"a - answer style (%s/%s vs. yes/no)\n",YES,NO);
	fprintf(stdout,"k - kills all questions until the next subject.\n");
	fprintf(stdout,"d - toggles between normal and dERanGeD text.\n");
	fprintf(stdout,"s - prints your current score for the test.\n");
#ifdef LOGFILE
	fprintf(stdout,"l - toggles whether or not your score is logged.\n");
#endif
	fprintf(stdout,"r - redraw current question.\n");
	fprintf(stdout,"q - quit the program.\n");
	fprintf(stdout,"? - print this help screen.\n\n");
}

int getresp()
{
	int ch;

	ch = fgetc(stdin);

	return (ch);
}

int subject()
{
	int err;

	the_s->fpos = ftell(fp) - 1;
	kill_sub = FALSE;

	if (fast) err = print (NO_ECHO,ENDSUB);
	else {
		fprintf(stdout,"------------------------------------------------------------------------------\n");
		fprintf(stdout, "\033[33;1m");
		fprintf(stdout,"%d. ",the_s->num);
		err = print(DO_ECHO,ENDSUB);
		fprintf(stdout, "\033[0m");
		(void) fputc('\n',stdout);
	}
	if (err == OK) {
		next_s = (struct quest *) malloc (sizeof(struct quest));
		if (next_s == NULL) {
			fprintf(stderr,"Memory allocation error.\n");
			err = EXIT;
		} else {
			next_s->last = the_s;
			next_s->num = the_s->num + 1;
			the_s = next_s;
		}
	}
	return (err);
}

int print(int echo, int esc)
	/* if echo then print the text, and give "more" prompts */
	/* the escape character to end this text block */
{
	int ch;

	while (TRUE) {
		ch = fgetc(fp);
		if (ch == EOF) return (FILE_ERR);
		if (ch == '\\') ch = fgetc(fp);
		else if (ch == esc) {	/* check next char for escapeness */
			return(OK);
		}
		if (echo) {
			if (derange)
				if ((ch >= 'a') && (ch <= 'z'))
					if (rand() % 2) ch += 'A' - 'a';
			if (rot13) {
				if ((ch <= 'z') && (ch >= 'a')) {
					if ((ch += 13) > 'z') ch -= 26;
				} else if ((ch <= 'Z') && (ch >= 'A')) {
					if ((ch += 13) > 'Z') ch -= 26;
				}
			}
			(void) fputc((char)ch,stdout);
			if (zoom == FALSE) {
				if (ch == '\n') {
					num_ch = 0;
					num_ln++;
				}
				if (num_ch >= cols) {	/* autowrap */
					num_ln++;
					(void) fputc('\n',stdout);
				}
				if (num_ln >= rows - 2) echo = more();
				if (echo == QUIT) return (QUIT);
			}
		}
	}
}

int more()
{
	int resp;
	long ret = LOOP;

	num_ch = num_ln = 0;
	fprintf(stdout,"--- more ---");
	while (ret == LOOP) {
		resp = getresp();
		switch (resp) {
			case ' ' :
			case 'y' :
			case 'Y' :
			case 'm' :
			case 'M' :
			case '\n' :
				ret = DO_ECHO;
				break;
			case 'n' :
			case 'N' :
			case 'k' :
			case 'K' :
				ret = NO_ECHO;
				break;
			case 'q' :
			case 'Q' :
			case 'x' :
			case 'X' :
				ret = QUIT;
				break;
			default :
				(void) fputc(007,stdout);
				break;
		}
	}
	fprintf(stdout,"\b\b\b\b\b\b\b\b\b\b\b\b            \b\b\b\b\b\b\b\b\b\b\b\b");
	return (ret);
}

void quit (int s __attribute__((unused)))
{
	(void) fflush(stdout);
	(void) signal (SIGINT, SIG_DFL);
	clr_mode();
	printscore();
	freemem();
	fprintf(stderr,"\nFine. Be that way. Don't use the 'q' option.\n");
	exit (0);
}

#ifdef SIGTSTP
void ctrlz (int s __attribute__((unused)))
{
	(void) fflush(stdout);
	fprintf(stdout,"\nyou'll be back, right?\n");
	clr_mode();
	(void) signal (SIGTSTP, SIG_DFL);
	(void) kill (getpid(), SIGTSTP);
}

void ctrlzret (int s __attribute__((unused)))
{
	fprintf(stdout,"nice to see you came back...\n");
	(void) fflush(stdout);
	set_mode();
}
#endif

void set_mode()
{
#ifndef SYSV
	struct sgttyb p;
#else
	struct termios p;
#endif

#ifndef SYSV
	(void) ioctl(0,TIOCGETP,&p);
	tty_flags = p.sg_flags;
	p.sg_flags |= CBREAK;
	p.sg_flags &= ~ECHO;
	(void) ioctl(0,TIOCSETP,&p);
#else
	tcgetattr(0,&p);
	tty_flags = p.c_lflag;
	baz = p.c_cc[VMIN];
	p.c_lflag &= ~ICANON;
	p.c_lflag &= ~ECHO;
	p.c_cc[VMIN] = 1;
	tcsetattr(0,TCSANOW,&p);
#endif
}

void clr_mode()
{
#ifndef SYSV
	struct sgttyb p;
#else
	struct termios p;
#endif

#ifndef SYSV
	(void) ioctl(0,TIOCGETP,&p);
	p.sg_flags = tty_flags;
	(void) ioctl(0,TIOCSETP,&p);
#else
	tcgetattr(0,&p);
	p.c_lflag = tty_flags;
	p.c_cc[VMIN] = baz;
	tcsetattr(0,TCSANOW,&p);
#endif
}

void printscore()
{
	int score = 0, num_q = 0;
	if (no_ans) return;

	nextq = theq->last;
	while (nextq != NULL) {
		if (nextq->answer == FALSE) score++;
		if (nextq->answer != SKIP) num_q++;
		nextq = nextq->last;
	}
	if (num_q <= 0) {
		fprintf(stdout,"\n\033[34;7myou can't get a score without answering any questions, silly.\033[0m\n\n");
		num_ln += 2;
	} else {
		fprintf(stdout, "\033[7m\nyou answered %d 'no' answer%s out of %d question%s,\n",score,(score -1) ? "s" : "",num_q,(num_q-1) ? "s" : "");
		fprintf(stdout,"which makes your purity score %.2f%%.\n\n\033[0m",
			((float) 100 * score / num_q));
		num_ln += 3;
	}
}

void freemem()
{
#ifdef LOGFILE
	int score = 0, ans = 0;
	char login[8], scorepath[512];
	struct passwd *pw;
#endif

	while (theq != NULL) {
		nextq = theq->last;
		free ((char *) theq);
		theq = nextq;
#ifdef LOGFILE
		if ((logme) && (no_ans == FALSE)) {
			if (theq != NULL) {
				if (nextq->answer == FALSE) score++;
				if (nextq->answer != SKIP) ans++;
			}
		}
#endif
	}
	while (the_s != NULL) {
		next_s = the_s->last;
		free ((char *) the_s);
		the_s = next_s;
	}

#ifdef LOGFILE
	if ((no_ans == FALSE) && logme && (ans > 0)) {
# ifdef SCOREFILE
		strcpy(scorepath, SCOREFILE);
# else
		sprintf(scorepath,"%s/%s",LIBDIR,"scores");
# endif
		fp = fopen (scorepath,"a+");
		if (fp != NULL) {
#ifdef GETLOGIN
			char *tmp = getlogin();
			(void) strcpy(login,(tmp) ? tmp : "");
			if (*login == '\0') {
#endif
#ifdef PW
				pw = getpwuid(getuid());
#else
				pw = getpw(getuid());
#endif
				(void) strcpy(login,pw->pw_name);
#ifdef GETLOGIN
			}
#endif
			fprintf (fp,"%s:%s: %d / %d = %.2f%%\n",login,testtype,
				score, ans, (float) ((100 * score) / ans));
			(void) fclose (fp);
		}
	}
#endif
}

int redraw()
{
	int err;

	err = fseek(fp,theq->fpos+1,0);
	if (err == -1) err = FILE_ERR;
	else {
		fprintf(stdout,"%3d. ",theq->num);
		err = print(DO_ECHO,ENDQUEST);
		fprintf(stdout,"\n");
	}

	return(err);
}

void sizewindow()
{
	struct winsize wsize;

	(void) ioctl(0,TIOCGWINSZ,&wsize);

	cols = wsize.ws_col;
	rows = wsize.ws_row;

	if (cols == 0) cols = 80;	/* if no row/col values, assign	*/
	if (rows == 0) rows = 24;	/* reasonable default ones.	*/
}

#ifndef PW
struct passwd *getpw(uid)
int uid;
{
	FILE *pwfp;	/* the password file pointer */
	struct passwd pw;
	char line[1024], *tmp, *tmp2;
	int count, ch;

	pwfp = fopen("/etc/passwd","r");
	if (pwfp == NULL) {
		return ((struct passwd *) NULL);
	} else {
		ch = fgetc(pwfp);
		count = 0;
		while (ch != EOF) {
			if (ch != '\n') {
				line[count++] = ch;
			} else {
				line[count] = '\0';
				count = 0;
				pw.pw_name = line;
#ifndef SYSV
				tmp = index(pw.pw_name,':');
				*tmp = '\0';
				pw.pw_passwd = ++tmp;
				tmp = index(pw.pw_passwd,':');
				*tmp = '\0';
				tmp2 = ++tmp;
				tmp = index(tmp2,':');
				*tmp = '\0';
				pw.pw_uid = atoi(tmp2);
				tmp2 = ++tmp;
				tmp = index(tmp2,':');
				*tmp = '\0';
				pw.pw_gid = atoi(tmp2);
				pw.pw_comment = ++tmp;
				tmp = index(pw.pw_comment,':');
				*tmp = '\0';
				pw.pw_dir = ++tmp;
				tmp = index(pw.pw_dir,':');
#else
				tmp = strchr(pw.pw_name,':');
				*tmp = '\0';
				pw.pw_passwd = ++tmp;
				tmp = strchr(pw.pw_passwd,':');
				*tmp = '\0';
				tmp2 = ++tmp;
				tmp = strchr(tmp2,':');
				*tmp = '\0';
				pw.pw_uid = atoi(tmp2);
				tmp2 = ++tmp;
				tmp = strchr(tmp2,':');
				*tmp = '\0';
				pw.pw_gid = atoi(tmp2);
				pw.pw_comment = ++tmp;
				tmp = strchr(pw.pw_comment,':');
				*tmp = '\0';
				pw.pw_dir = ++tmp;
				tmp = strchr(pw.pw_dir,':');
#endif
				*tmp = '\0';
				pw.pw_shell = ++tmp;
				if (pw.pw_uid == uid) {
					fclose(pwfp);
					return (&pw);
				}
			}
			ch = fgetc(pwfp);
		}
		fclose(pwfp);
		return(NULL);
	}
}
#endif
