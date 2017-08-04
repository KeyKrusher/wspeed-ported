#include <stdlib.h>
#include <string.h>

#include "wspeed.h"

#include "musa02.c"
#include "musa03.c"

char inputline[17];
long score;
int inplen;
int mokia;
int kirjaimia;     //pelaajan kirjoittamia
int last10mrs;     //edellinen 10mrs
int curr10mrs;     //t„m„nhetkinen -"-
int cached1;       //edellinen -"-"-
int cached2;       //edellinen -"-"-

int sanoja;        //pelaaja kirjoittanut
long beenrunning;  //peli ollut k„ynniss„, sadasosasekunteina

static void spawnword(void)
{
    int a, b, c;
    // Etsit„„n oliobufferista vapaa reik„
    for(a=0; a<maxitem; a++)if(!items[a].typi)break;
    // Jos reik„ l”ytyi,
    if(a-maxitem && used<sanac)
    {
        unsigned char Limit = NoCollision ? 1 : 255;
        // tehd„„n uusi sana,
        items[a].typi = sana;
        do
            items[a].param = random(sanac);
        while(sanau[items[a].param] >= Limit);

        // jonnekin pystyriville, ruudun vasemman reunan vasemmalle puolelle.
        items[a].x = -WordLen(items[a].param);
        items[a].y = rand()%Riveja;

        // Tarkistetaan, ettei se mene jonkin toisen sanan p„„lle.
        for(b=c=0; b<maxitem; b++)
            if(items[b].typi==sana)
                if(items[b].y==items[a].y
                && items[b].x < 2 // T„ytyy olla kaksi merkki„ tilaa...
                && a!=b)
                {
                    // Menee p„„lle, kokeillaan seuraavaa rivi„...
                    items[a].y=(items[a].y+1)%Riveja;
                    b=-1;
                    // Jos on jo kokeiltu kaikkia rivej„,
                    if(c++ >= Riveja)
                    {
                        // sitten ei voi mit„„n - sana ei mahdu ruudulle.
                        items[a].typi = tyhjyys;
                        break;
                    }
                }

        // Jos sana luotiin,
        if(items[a].typi == sana)
        {
            // p„ivitet„„n k„ytettyjen sanojen tilastoja.
            used++;
            sanau[items[a].param]++;
        }
    }
}

static void sanatilanne(void)
{
    int b = Paljonkoruudulla();
    // nyt b=kirjaimien lukum„„r„ n„yt”ll„

    // Tehd„„n lis„„ sanoja, jos k„ytt„j„ pystyisi
    // kirjoittamaan niit„ enemm„nkin.

    // Yksitt„inen kirjain viipyy ruudulla
    // keskim„„rin 0.01 * SANAVIIVE * 80 sekuntia...
    // Se on siis SANAVIIVE*8/10 sekuntia.
    // Pelaaja kirjoittaa keskim„„rin last10mrs
    // merkki„ sekunnissa. Se tarkoittaa, ett„
    // SANAVIIVE*8 sekunnissa pelaaja kirjoittaa
    // suunnilleen last10mrs*8*SANAVIIVE/10/600 merkki„,
    // eli last10mrs*SANAVIIVE/750 merkki„.
    // Jos t„m„ arvo (k„ytt„j„n nopeus) on suurempi kuin
    // n„yt”ll„ olevien merkkien m„„r„ t„ll„ hetkell„,
    // pit„„ antaa lis„„ haastetta.

    if(RequireBetter) b -= kirjaimia/RequireBetter;

    if(b < CurrentSkill() || !used)spawnword();
    // Spawnataan my”s, jos ruudulla ei ole yht„„n sanaa.
}

static int checkinput(void)
{
    int a, b=0;
    for(a=0; a<maxitem; a++)
        if(items[a].typi == 2)
        {
            const char *word = GetWord(items[a].param, NULL);
            if(stricmp(word, inputline) == 0)
            {
                if(!Training)
                {
                    int add = inplen + 79-items[a].x;
                    if(strcmp(word, inputline) != 0)
                    {
                        // Jos oli isoja/pieni„ kirjaimia
                        // v„„rin, annetaan vain puolet pisteet.
                        add /= 2;
                    }
                    // Anna pisteit„ nopeuden mukaan.
                    score += add;
                }
                // Ja kirjaimien m„„r„ sanan pituuden mukaan.
                kirjaimia += inplen;
                items[a].typi = tyhjyys;
                if(!--sanau[items[a].param])used--;
                b++;
            }
        }
    return b;
}

int GamePlay(void)
{
    int a, Escaped=0;

    sanoja=kirjaimia=last10mrs=0;

    cached1=cached2=curr10mrs=0;

    memset(Buffer, 0, sizeof Buffer);

    // Kursori kohdalleen heti pelin alussa,
    // mutta annetaan pklitelle mahdollisuus.
    gotoxy(2+inplen, 25);

    beenrunning = 0;
    for(score=inplen=used=mokia=0;;)
    {
        #ifdef __BORLANDC__
        initdelay(10); // 10=beenrunning mittaa oikein
        #endif

        sanatilanne();

        beenrunning&15||spawnstar();

        updstatus();        // gprintfi„
        drawscreen(BParm);  // Palauttaa montako sanaa oli liikenteess„

        #ifdef __BORLANDC__
        outportb(0x3C8, 0);
        outportb(0x3C9, 0);
        outportb(0x3C9, 0);
        outportb(0x3C9, 0);
        #endif

        if(mokia > 9)break; // Ruudun piirron j„lkeen vasta t„m„ tarkistus.

        if(WS_kbhit())
        {
            a = WS_getch();
            switch(a)
            {
                case 0:
                    switch(WS_getch())
                    {
                        case '-': goto P2;
                        case ';': Help();
                    }
                    break;
                case 8:
                    if(inplen)inputline[--inplen]=0;
                    break;
                case 27:
                    Escaped = 1;
                    goto P2;
                case '!': NoDelay=!NoDelay; break;
                case ' ':
                case '\r':
                case '\n':
                    sanoja += checkinput();
                    Laske10mrs();
                    inputline[inplen=0]=0;
                    break;
                default:
                    if(a>' ' && inplen < 16 && a!=':')
                    {//don't ask, don't modify the ':'
                        inputline[inplen++] = a;
                        inputline[inplen] = 0;
                    }
            }
            gotoxy(2+inplen, 25);
        }
        if(!NoDelay)
        {
            #ifndef __BORLANDC__
            delay(10);
            #else
            if(DelayCount>0)
            {
                a = DelayCount>63?63:(int)DelayCount;
                outportb(0x3C8, 0);
                outportb(0x3C9, 63);
                outportb(0x3C9, a);
                outportb(0x3C9, a);
            }
            while(DelayCount<0);
            #endif
        }
        beenrunning++;
    }

P2: ;

    for(a=0; a<maxitem; a++)if(items[a].typi == 2)items[a].typi = tyhjyys;

    return Escaped;
}

static char *Date(time_t When)
{
    struct tm *TM = localtime(&When);
    static char Gaa[] = "000000 00:00:00";
    if(When)
        strftime(Gaa, sizeof Gaa, "%d%m%y %H:%M:%S", TM);
    else
        strcpy(Gaa, "...... ..:..:..");
    return Gaa;
}

int GameOver(int Escaped)
{
    static char PlayerName[MaxScoreNameLen+1] = "anonymous";

    int x, y, z, c, Lines, Depf;

    gprintf(0, 24, 0x4F, SCREEN, "%43s%37s", "GAME OVER", "");

    for(z=0; z<=80; z++)
    {
        register unsigned short c = (z<40)?0x4720:0;
        if(z<40)x=z;else {StarDir=1;x=80-z;}
        for(y=0; y<24; y++)BParm[x+y*80]=BParm[79-x+y*80]=c;
        GenIdle;
        if(MusData.GlobalVolume>0)MusData.GlobalVolume--;
    }

    if(!Escaped)
    {
        MusData.GlobalVolume = 64;
        StartS3M(adl_end);
        gprintf(0, 24, 0x4E, SCREEN, "Press ESC to continue");
        gotoxy(22, 25);
        for(;;)
        {
            while(!WS_kbhit())GenIdle;
            if(WS_getch()==27)break;
        }
    }

    gprintf(0, 24, 0x46, SCREEN, "Game %-25s", GetModel());

    MusData.GlobalVolume = 40;
    StartS3M(adl_gameover);

    ReadScores();

    Depf = ScoreDepth;

    for(x=0; x<ScoreDepth; x++)
        if(!Scores[x].Score)
        {
            Depf=x+1;
            break;
        }
    Lines = Depf>15?15:Depf;

    /* Depf kertoo, kuinka monta scorea listassa on
     * Lines kertoo, kuinka monta rivi„ n„yt”ll„ olevassa
     * scoretablessa on kerrallaan */

    if(!score)
        x=Depf-1;
    else
        for(x=0; x<ScoreDepth; x++)
            if(score > Scores[x].Score || !Scores[x].Score)
            {
                for(y=ScoreDepth-1; y>x; y--)Scores[y] = Scores[y-1];
            //  for(y=x; y<ScoreDepth-1; y++)Scores[y+1] = Scores[y];
                Scores[x].Score     = score;
                Scores[x].kirjaimia = kirjaimia;
                Scores[x].last10mrs = last10mrs;
                Scores[x].Sanoja    = sanoja;
                time(&Scores[x].When);
                strcpy(Scores[x].Name, PlayerName);
                break;
            }

    MusData.GlobalVolume = 64;

    #define Line1 5
    #define Colu1 0

#define Putta(Color, DispY, Index) \
    gprintf( \
        Colu1, (DispY)+Line1, Color, BParm, \
        "º%2u³%6lu³%6d³%5d³%5d³%*s³%sº", \
        (Index)+1, \
        Scores[Index].Score, \
        Scores[Index].kirjaimia, \
        Scores[Index].last10mrs, \
        Scores[Index].Sanoja, \
        MaxScoreNameLen, \
        Scores[Index].Name, \
        Date(Scores[Index].When));

    c = x;      // Own score position, ScoreDepth=no space.
    z = c - 7;  // Top line (scroll position)
    //x = c;    // Highlighted line

#define Draw \
    if(z<0)z=0; \
    if(z>Depf-Lines)z=Depf-Lines; \
    gprintf(Colu1, Line1-1, 0x1F, BParm, \
        "É #ÑScoreÍÑNum àáÑ10mrsÑwordsÑÍ¹    The high score records   ÌÍÑDatetimeÍÍÍÍÍÍÍ»"); \
    for(y=0; y<Lines; y++)Putta(0x1F, y, z+y); \
    gprintf(Colu1, y+Line1, 0x1F, BParm, \
        "ÈÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ¼");

    if(c < ScoreDepth && score) /* T„ytyy sent„„n v„h„n pisteit„ olla. */
    {
        int Len = strlen(PlayerName);
        Draw
        for(;;)
        {
            Putta(0x3E, x-z, x);
            gprintf(
                Colu1+30,
                (x-z)+Line1, 0x3F, BParm,
                "%*s", -MaxScoreNameLen, PlayerName);
            gotoxy(Colu1+30+1+Len, x-z+Line1+1);
            GenIdle;
            if(WS_kbhit())
            {
                int k = WS_getch();
                switch(k)
                {
                    case 0:
                        WS_getch();
                        break;
                    case 8:
                    case 127:
                        if(Len)
                            PlayerName[--Len] = 0;
                        break;
                    case 27:
                        ReadScores();
                        goto Rescroll;
                    default:
                        if(k >= ' ' && k < 255 && Len<MaxScoreNameLen)
                        {
                            PlayerName[Len] = k;
                            PlayerName[++Len] = 0;
                        }
                        break;
                    case '\n':
                    case '\r':
                        goto Written;
                }
            }
        }
Written:
        strcpy(Scores[x].Name, PlayerName);
        WriteScores();
    }

    int Local, Global;
    FindAverage10mrs(PlayerName, &Local, &Global);

    if(Global)
    {
        gprintf(0, 22, 0x1B, BParm, "%-80s", "");
        gprintf(0, 22, 0x1B, BParm,
            "Your average ten minute relative speed in this test is %d. Overall %d.",
            Local, Global);
    }

Rescroll:
    unsigned char far *a = ((unsigned char far *)SCREEN) + 23*160;
    int n;
    for(n=0; *a!='('; n++, a+=2);
    gprintf(n, 23, 0x1F, SCREEN, "%*s", 80-n, "(F1 to reselect word libraries)");

    Draw
    gotoxy(13, 25);
    Putta(0x3E, x-z, x);

//  x=0;

    for(;;)
    {
        gprintf(0, 24, 0x4F, SCREEN, "%-30s", "Play again?");
        GenIdle;
        if(WS_kbhit())
        {
            int k = WS_getch();
            switch(k)
            {
                case 27:
                case 'n':
                case 'N':
                    goto Ohi;
                case 'y':
                case 'Y':
                case '\n':
                case '\r':
                    StarDir = -1;
                    return 1;
                case 0:
                    switch(WS_getch())
                    {
                        case 'H':
                            if(x>0)x--;
                            if(x<z)z=x;
                            goto Rescroll;
                        case 'P':
                            if(x<Depf-1)x++;
                            if(x>=z+Lines)z=x-Lines+1;
                            goto Rescroll;
                        case ';': //F1
                            StarDir = -1;
                            return 2;
                    }
            }
        }
    }
Ohi:
#undef Putta

    StarDir = -1;

    gprintf(0, 24, 7, SCREEN, "%80s", "Closing down.");

    return 0;
}

int GameLoop(void)
{
    for(;;)
    {
        MusData.GlobalVolume = 64;
        StartS3M(GameMusic);
        /* Check the score file       *
         * before starting to play... */
        ReadScores();
        switch(GameOver(GamePlay()))
        {
            case 0: return 0;
            case 2: return 1;
        }
    }
}
