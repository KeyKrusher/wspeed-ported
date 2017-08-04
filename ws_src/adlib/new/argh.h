#ifndef _b_ARGH_H
#define _b_ARGH_H

#ifdef __cplusplus
extern "C" {
#endif

/* To be defined by user program */
extern void DefArg(const char *arg);
extern void Case(int id, const char **dummy);
extern void PrintVersionInfo(void);
extern void PrintDescription(void);
extern const char ProgName[];
extern struct Option
{
	int ID;
	char c, c2;
	const char *txt, *descr;
} Options[];

/* Defined in argh (argument handler) */
extern int Heelp;
extern long ParamRange(long min, long max, const char **s);
extern void SuggestHelp(void);
/* ArgError:
   0: Illegal option: %c
   1: Unrecognized option: --%s
   2: Invalid parameter: `%s'
*/
extern void ArgError(int a, const char *b);
extern void ReadArgs(int argc, const char *const *argv,
                     const char *UsageExtra,
                     const char *UsageEnd);

#ifdef __cplusplus
}
#endif

#endif	/* _b_ARGH_H defined */
