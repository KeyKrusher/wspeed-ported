#ifndef bqMusicArch_h
#define bqMusicArch_h

/* PlayArchive():
     index==0:
        Soittaa Defaultin
     index>0:
        Soittaa biisin numero <index>
     index<0
        Kertoo biisien määrän tiedostossa.
*/
extern int PlayArchive(int index, const char *FileName, const char *Default);

#endif

