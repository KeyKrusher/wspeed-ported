#include <stdio.h>
#include <stdlib.h>
#include "music.h"

/* Used in StopS3M too */
int FromArchive = 0;
char *Archived = NULL;

int PlayArchive(int index, const char *FileName, const char *Default)
{
    unsigned int a, b, c;
	FILE *fp;

	StopS3M();

    FromArchive=1;

    if(!index)
	{
        StartS3M(Default);
        return 0;
	}

    fp = fopen(FileName, "rb");

	if(!fp)
	{
        if(index >= 0)StartS3M(Default);
        return 0;
	}

    for(c=0; ; c++)
	{
		a = fgetc(fp);
		b = fgetc(fp);

        if(!a && !b)break;
        if(index > 0)if(!--index)break;

		fseek(fp, a|(b<<8), SEEK_CUR);
	}

    if(!index)
    {
        Archived = (char *)malloc(a | (b << 8));
    
        fread(Archived, 1, a | (b << 8), fp);

        StartS3M(Archived);
    }

    FromArchive=0;

	fclose(fp);

    return c;
}
