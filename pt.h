/*
	pt.h		Eric Lechner		December 1989

	files, definitions, and structures for pt.c,
	including the escape character definitions,
	as well as certain message definitions.

	edit all entries as necessary.  pay particular
	attention to all directory and file path entries.
*/

/*
    SYSV define removed, and SYSV and LINUX defines moved to Makefile
	

*/

/*
    GID: if defined, the program will set its group id to GID
*/
#define GID 13

/*
    LOGFILE: determines whether or not to save scores.
	if it is defined, scores will be kept in the
	$(LIBDIR)/scores file.
*/
#define LOGFILE

/*
    MASK: the string that is copied into argv[0] as a command mask.
	(if not defined, then no mask is performed)
*/
#define MASK "purity testing!                            "

/*
    GETLOGIN: if you want username checking to check getlogin()
	before trying getpwuid(getuid()), define GETLOGIN.
*/
#define GETLOGIN

/*
    PW: if you want the purity test to use getpwuid(getuid())
	instead of my hacked (MUCH smaller on our system)
	routine, define PW.
*/
#define PW

/*
    How text is printed in the purity test:

    Blocks of plain text (like for introductions):
	anything enclosed within TEXT and ENDTEXT characters is printed.

    Subjects headings:
	the subject number printed, then the block of text enclosed in
	SUBJECT and ENDSUB characters.

    Questions:
	anything within the characters QUESTION and ENDQUEST are printed
	as a question, with its number preceeding it, and with a prompt
	for the answer afterwards.

    End of test, time to print score, and then the conclusion:
	the score for the test is printed, and then the text
	within the PRINTSCORE and ENDSCORE characters is printed.
*/
#ifdef LINUX
#   define TEXT		'{'	/* open squiggly  */
#   define ENDTEXT	'}'	/* close squiggly */
#   define SUBJECT	'['	/* open brace	  */
#   define ENDSUB	']'	/* close brace	  */
#   define QUESTION	'('	/* open paren	  */
#   define ENDQUEST 	')'	/* close paren	  */
#else
#   define TEXT		'\{'	/* open squiggly  */
#   define ENDTEXT	'\}'	/* close squiggly */
#   define SUBJECT	'\['	/* open brace	  */
#   define ENDSUB	'\]'	/* close brace	  */
#   define QUESTION	'\('	/* open paren	  */
#   define ENDQUEST 	'\)'	/* close paren	  */
#endif

#define PRINTSCORE	'<'	/* open brocket   */
#define ENDSCORE	'>'	/* close broket   */

/* the following are what the test prints for you answer	*/
/* instead of "yes" and "no" for purposes of obfuscating the	*/
/* printed answers the questions.				*/

/* YES: what the test echoes when the user answers 'yes' to a question. */
#define YES "Maybe"

/* NO: what the test echoes when the user answers 'no' to a question. */
#define NO "maybe"

/*	end of the purity test library file	*/
