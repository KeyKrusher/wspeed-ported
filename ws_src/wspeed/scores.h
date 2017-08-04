#ifndef __SCORES_H
#define __SCORES_H

#include <time.h>

#define MaxScoreNameLen 33
typedef struct ScoreItem
{
	long Score;
	int Mokia;
	int kirjaimia;
	int last10mrs;
	int Sanoja;
	time_t When;
	char Name[MaxScoreNameLen+1];
}ScoreItem;
/* Score depth is the maximum number of   *
 * score records for single model. Now    *
 * it is good that it does not exceed 99. */
#define ScoreDepth 50
extern ScoreItem Scores[ScoreDepth];

extern void ReadScores(void);
extern void WriteScores(void);
extern char *GetModel(void);

/* Local and Global store the results */
extern void FindAverage10mrs(const char *Who, int *Local, int *Global);

#define perror(s) \
	textattr(7); \
	cprintf("\r\n\n%s: %s (errno %d)\r\n", (s), sys_errlist[errno], errno);

#endif
