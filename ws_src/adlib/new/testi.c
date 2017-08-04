#include <stdio.h>
#include "hspc.c"

#include "m_opl.h"
#include "m_mpu.h"

int main(void)
{
	if(OPLDriver.detect() < 0)
	{
		fprintf(stderr, "OPL not detected!\n");
		return -1;
	}
	MusData.driver = &OPLDriver;
	
	StartS3M(hspc);
	
	printf("Playing, newline to end\n");
	fgetc(stdin);
	
	StopS3M();
	
	printf("Ending\n");
	
	MusData.driver->close();
	
	return 0;
}
