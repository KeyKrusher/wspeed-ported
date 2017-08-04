#include <conio.h>

#include "wspeed.h"

static int zeroflag=0;
int WS_kbhit()
{
    return kbhit();
}
int WS_getch()
{
    int c = getch();
    if(zeroflag) { zeroflag=0; return c; }
    if(!c) { zeroflag=1; return c; }
    if(Dvorak)
        switch(c)
        {
            /* THIS TRANSLATES FINNISH QWERTY TO FINNISH DVORAK.


                 «1234567890+`           «1234567890+`
                  QWERTYUIOP^            ,.PYFGJRL<^
                  ASDFGHJKL™'   becomes  OAEUIDHTNS-'
                  >ZXCVBNM;:_             Q™CKXBMWVZ

            */

            case 'q': return ',';    case 'Q': return ';';
            case 'w': return '„';    case 'W': return '';
            case 'e': return '.';    case 'E': return ':';
            case 'r': return 'p';    case 'R': return 'P';
            case 't': return 'y';    case 'T': return 'Y';
            case 'y': return 'f';    case 'Y': return 'F';
            case 'u': return 'g';    case 'U': return 'G';
            case 'i': return 'j';    case 'I': return 'J';
            case 'o': return 'r';    case 'O': return 'R';
            case 'p': return 'l';    case 'P': return 'L';
            case '†': return '<';    case '': return '>';
            case 'a': return 'o';    case 'A': return 'O';
            case 's': return 'a';    case 'S': return 'A';
            case 'd': return 'e';    case 'D': return 'E';
            case 'f': return 'u';    case 'F': return 'U';
            case 'g': return 'i';    case 'G': return 'I';
            case 'h': return 'd';    case 'H': return 'D';
            case 'j': return 'h';    case 'J': return 'H';
            case 'k': return 't';    case 'K': return 'T';
            case 'l': return 'n';    case 'L': return 'N';
            case '”': return 's';    case '™': return 'S';
            case '„': return '-';    case '': return '_';
            case '<': return '†';    case '>': return '';
            case 'z': return 'q';    case 'Z': return 'Q';
            case 'x': return '”';    case 'X': return '™';
            case 'c': return 'c';    case 'C': return 'C';
            case 'v': return 'k';    case 'V': return 'K';
            case 'b': return 'x';    case 'B': return 'X';
            case 'n': return 'b';    case 'N': return 'B';
            case 'm': return 'm';    case 'M': return 'M';
            case ',': return 'w';    case ';': return 'W';
            case '.': return 'v';    case ':': return 'V';
            case '-': return 'z';    case '_': return 'Z';
        }
    return c;
}
