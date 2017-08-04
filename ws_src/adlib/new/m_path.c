#include <string.h>
#include <stdlib.h>
#ifdef __GNUC__
#include <unistd.h>
#else
#include <io.h>
#endif
#ifndef __BORLANDC__
#include <ctype.h>
#endif

char *Pathi(char *dest, int maxlen, const char *env, const char *what)
{
	char buf[512];
	char buf2[128];
	char *p, *s, *path;
	int i;

	strcpy(buf, env);
    #ifdef __BORLANDC__
	strupr(buf);
	#else
	for(s=buf; *s; s++)*s = toupper((int)*s);
	#endif
	path = getenv(buf);

    if(path && access(what, 4) && strlen(path) < sizeof(buf))
    {
        for(strcpy(p=buf, path);;)
        {
            #if defined(__MSDOS__) || defined(MSDOS)
            s = strchr(p, ';');
            #else
            s = strchr(p, ':');
            #endif
            if(s)*s=0;
    
            strcpy(buf2, p);
            i=strlen(buf2);
            if(i)
            {
                if(buf2[i-1]!='\\')strcat(buf2, "\\");
                strcat(buf2, what);
                if(!access(buf2, 4))
                {
                    what=buf2;
                    break;
                }
            }
            if(!s)break;
            p=s+1;
        }
    } /* iso if */
    return strncpy(dest, what, maxlen);
}
