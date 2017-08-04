static char *Title = "NES-S3M player for /dev/audio (µ-law)\n"
"Copyright (C) 1992,1999 Bisqwit (http://www.icon.fi/~bisqwit/)";
static unsigned At[100][64],NESPos=0,LS=0,P[12],H[99],D[99],IL[99],IV[99];
typedef unsigned char byte;byte *z;
char*A="/dev/audio";char*g=" [> output]\n",*tmp="nesmusa.$$$";static byte
o[256],w[256][64],s[5][65536],l[299999],NN[8],sn[32]="clip from Goonies 2";
#define rw(a) a=fgetc(in),a|=fgetc(in)<<8
#include <stdio.h>
#include <unistd.h>
FILE*fo,*in=stdin;
static int ss,S=SEEK_SET,ON,IN,PN,IP[256],PP[256],r,i,a,c,CO,CP,j,x,T=150;
static int Out(int v){int e=0,s=1,f;for((s=(v<0))?v=-v:0;e<8;e++)if((f
=((v+32)>>(e+1))-16)<16)break;return fputc(((s<<7)|(e<<4)|f)^255,fo);}
int main(int C,char**V)
{
    for(P[CO=a=i=C?0:(fseek(fo,0,SEEK_END),r=ftell(fo)/8,rewind(fo),
        fread(l,8,r,fo),fclose(fo),remove(tmp),(z=(byte*)(in==stdin
        ?"stdin":V[1])),main(-1,0))]=907;++i<12;P[i]=P[i-1]*89/84);
    if(C<0)
    {
        if(isatty(fileno(fo=stdout)))fo=fopen(A,"wb");
        if(!fo){perror(A);exit(-1);}a=c=i=sn[29]=0;
        for(fprintf(stderr,"Playing %s (%s)...\n",z,sn);;
            i<3?i++:((c?c--^Out(S/10):++a>=r?a=LS:(c=20000/T)),i=S=0))
        {
            x=l[a*8+4+i],j=i?i-1:0;
            H[i] += (P[x%12] * (D[j]?D[j]:14500)/14500) << (x/12);
            if(IL[j])S += (s[j][(H[i]/8000)%IL[j]]-128)*l[a*8+i];
    }   }
    if(C<2)
    {
        printf("%s\n\nUsage:\nnesplay nesfile.s3m%snesplay -%s\n",Title,g,g);
        for(r=1280,LS=128,c=4;c--;)
            for(a=0;a<r;z++)
                if((S=*z++^255) != 2)l[c+8*a+4]=S,l[c+8*a++]=*z;
                else for(i=(*z++^130)*256,S=(i+=(*z^130))&63,i/=64;S--;a++)
                    l[a*8+4+c]=l[(a-i)*8+4+c],
                    l[a*8+  c]=l[(a-i)*8+  c];
        for(i=IL[0]=IL[1]=32;--i>=0;s[0][i]=(i&24)?170:20)s[1][i]=i<16?i*16:(31-i)*16;
        for(i=IL[2]=4096;--i>=0;s[2][i]=(a=a*999+1)%200);z=(byte*)"built-in song";main(i,0);
    }
    if(*V[1]!='-')in=fopen(V[1],"rb");if(!in||!(fo=fopen(tmp,"wb+")))
    {perror(in?tmp:V[1]);return-1;}fread(sn,1,32,in);rw(ON);rw(IN);
    rw(PN);fseek(in,49,S);x=fgetc(in);T=fgetc(in);fseek(in,0x60,S);
    fread(o,ON,1,in);for(i=0;i<IN;i++)rw(IP[i]);
    for(i=0;i<PN;i++)rw(PP[i]);
    for(i=0;i<IN;i++)
        fseek(in,IP[i]*16+13,S),fgetc(in),rw(LS),rw(IL[i]),rw(j),
        fseek(in,8,SEEK_CUR),IV[i]=fgetc(in),fseek(in,3,SEEK_CUR),
        rw(D[i]),fseek(in,LS*16,S),fread(s[i],1,IL[i],in);
    for(;;){
    if(CO>=ON)main(0,V);if((CP=o[CO])-254){
    fseek(in,PP[CP]*16,S);rw(i);if(!i)PP[CP]=0;
    if(PP[CP])fread(z=s[4],i-=2,1,in);
    for(r=0;r<64;r++)
    {
        int b,Cmd,I,Bxx,Cxx,B,Ss;
        if(!r)ss=1;
        a?0:(w[CO][r]?(LS=At[CO][r]),main(0,V):(At[CO][r]=ftell(fo)/8,(w[CO][r]=1)));
        Bxx=Cxx=-1,Ss=ss,j=B=ss=0;
        if(PP[CP])while((b=*z++)-(Cmd=I=0))
        {
            if(b&32){if(!a){if(*z<254)NN[(b&3)+4]=*z%16+12*(*z/16);else
            if(*z==254)NN[b&3]=0;if(z[1]&&*z!=254)NN[b&3]=IV[z[1]-1];}z+=2;}
            if(b&64){if(!a)NN[b&3]=*z;z++;}if(b&128){Cmd=*z++;I=*z++;}
            if(Cmd==1&&!a)x=I;Cmd==2?Bxx=I:Cmd==3?Cxx=I:Cmd==4?ss=1:0;
            if(Cmd==19)
            {
                if(I/16==11){if(!(B=I&15))Ss=1;else ss=1;}
                if(I/16==14)j=I&15;
            }
        }
        if(a)a--;else
        {
            if(Ss)NESPos=ftell(fo);
            for(i=(j+1)*x;i;i--)fwrite(NN,1,8,fo);
            if(B){c=ftell(fo)-NESPos;fseek(fo,NESPos,S);fread(l,1,c,fo);
            fseek(fo,NESPos+c,S);for(i=B;i;i--)fwrite(l,1,c,fo);}
            if(Bxx>=0){CO=Bxx;if(Cxx>=0)a=(Cxx&15)+10*(Cxx>>4);CO--;break;}
            if(Cxx>=0){a=(Cxx&15)+10*(Cxx>>4);break;}
        }
    }}
    CO++;}
}
byte*z=(byte*)"¹\4¹\4¹\4¹\0ı‚ıı’½ı}ı­=ı½ııÍ½ıÜ}ıì=ıüıı\14½ı\37}ı/=ı?ııO½ı^"
"}ın=ı~ıı}}ı}}ı}}ı}Rş\0ı‚ıı’˜ã0ı‚Çâ0ı‚ıı’½ıyá0ı‚ıı’½ı}á0á0ıâ½ıìııü½ıâ½ıí½ıü"
"}ı\14\4ß0ı‚ıı’½ı}ß0ß0Ş0ı‚ıı’¢ı}NÖ0ı‚ÉØ0ı‚ÉÚ0ı‚Éı„šÛ0ı‚Éı¯ıB¤ş\0ı‚ıı’˜Ö\24Ö"
"\20ı‚ÈÑ\24Ñ\20ı‚ÈÏ\24Ï\20ı‚ÈÊ\24Ê\20ı‚Èı½ıš½ı¦½ı²½ıÊ½ıÖ½ıâ½ıú½ı\6½ı\22½ı*½ı"
"6½ıB½ıZ½ıf¨Ò\24Ò\20ı‚Èı½ı~‰Ö\14ı‚ËÊ\24Ê\20Ê\14ı‚ËÌ\24Ì\20Ì\14ı‚ËÎ\24Î\20Î"
"\14ı‚Ëı„šÏ\24Ï\20Ï\14ı‚ËÒ\24Ò\20Ò\14ı‚Ëı~¤ì\4Ô\14Ô\14Ô\0æ\14æ\20æ\24å\30å\34"
"å ä$ã(â,à0Ş4Ü8×<×\0ı‚Éâ<î<ö<ö\0ö\0ö\0Û8ç8î8Ù4å4ì4Û0ç0î0Ù,å,ì,Û(ç(î(Ù$å$ì$Û ç"
" î Ù\34å\34ì\34Û\30ç\30î\30Ù\24å\24ì\24Û\20ç\20î\20î\0ı‚ãÖ\10ı‚ÉÑ\10ı‚ÉÏ\10ı"
"‚ÉÊ\10ı‚Éı½ıš½ı¦½ı²½ıÊ½ıÖ½ıâ½ıú½ı\6½ı\22½ı*½ı6½ıB½ıZ½ıf¨Ò\10ı‚Éı½ı~—ıtÌ"
"\10ı‚ÉÎ\10ı‚Éı„šı~ı¨ı~˜";
