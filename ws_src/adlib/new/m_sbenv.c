#include <stdlib.h>

#include "m_sbenv.h"

struct sbenv sbenv;

void fillsbenv()
{
    char *envvar = getenv("BLASTER");
    if(!envvar)
        sbenv.set = 0;
    else
    {
        sbenv.set = 1;
        sbenv.A = sbenv.D = sbenv.I 
                = sbenv.H = sbenv.E
                = sbenv.T = sbenv.P = -1;

        while(*envvar)
        {
            if(*envvar=='A')sbenv.A = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='D')sbenv.D = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='I')sbenv.I = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='H')sbenv.H = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='P')sbenv.P = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='E')sbenv.E = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='T')sbenv.T = (int)strtol(envvar+1, NULL, 16);
            
            /* These extensions are no longer supported */
            #if 0
            if(*envvar=='R')ReverbLevel = (int)strtol(envvar+1, NULL, 10)*127/100;
            if(*envvar=='C')ChorusLevel = (int)strtol(envvar+1, NULL, 10)*127/100;
            #endif
            
            if(!sbenv.E && sbenv.A>0)sbenv.E=sbenv.A+0x400;
            while((*envvar!=' ')&&(*envvar!=0))envvar++;
            if(*envvar)envvar++;
        }
    }
}
