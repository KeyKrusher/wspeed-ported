#define _POSIX_SOURCE 1 /* Song length: 41 seconds (37 seconds of loop). */
/*/echo "$0 is not an executable but a C source file. Compiling it now..."
n=`basename $0`;gcc -O2 -Wall -W -pedantic -o /tmp/$n "$0";/tmp/$n $*;exit; */
static char*Title="NES-S3M player for /dev/audio (8-bit, µ-law)\n"
"This file has been compiled from a file created with intgen.";
typedef unsigned char by;char*A,*In,*tmp="nesmusa.$$$",*a0;
static by*z=(by*)"n4coXAUemK*b*LE5BJ(<B*>N*)6PgGQ[D]_dAoM2*<b*GZJ^1CIXLW_1am:"
"*NZ+dJK)ZJ+*]*<(n7b<ZMJm>]B<XnWc<YMTm5B*)^OCAfdQ_OAmdl=J+8N*>lm*2om+Jnm)*nm5"
"*lm=*nmM*bmm*Zmm+Jmm)*](UL<6*9N`mfmSm8=5(*00oE`]fmRU)+*7Il5o=`EfYK*J(;mHl7oM"
"R0Bb*)nm+6PQ)W4)5H?4<F@>NJ+lm*(G[KDWK)>*G5Z8^=J+RL*Fmm*=6b+*lm)*nm5*b[77O2*2"
"b**]UP=GMPlFoS@>g9P]FMSl8oW@fgIQ=EM\\lNoc@Vg)Q=DM^lBo[@FgY+>bW)n*4WL0oG`QfES"
"]8IW<1_D@_fASc8eW\\1_E`m:*VZ+>*I)0*)5b+B4o>`a:*NZR[;UID=Z*2O*B*lfYS]9MUl<oO@"
"nfcSg9QUL=?M`lfoSo9m5(J6<J+BN:+Bb*=:X+`HP)*)A5*S(<:*KN2RFZfIS])+*3T4?O@@gfPS"
"E9YT\\?OA@ef]R5;M5(J)6?0`Ff`+4JD==Ll<6*5bCfYSUg)>*_JL*oO2*<*f2R9:Q5B*@en^ca:"
"*NZT[?UAD=Z*FSc;YeK*b*?I@<g^LJ+R`**mm+*lm)*nm@4gNPC_f9S]8MWl0oG@^fCSg8QWL1?E"
"`\\fOSo8m5(J6<J+BN:+Bb*=FHQ=EM\\lNoc@Vg)Q=DM^lBo[@>d9^]BM[lHo7AngYQ]EM]lLoo@"
"fgY+>B5G=PlFoS`9gUP[F=Sl8oW`1gEP[)@;+5nH(BOX@Wg0QEDY^\\COY@UgmK*RJ)B*45>*4<B"
"34N*6Vb*BBJP9GQPDGoP@Dg_PAGaPl=6*3N`cfeK*bJ9IU<=_LZ=0bJJF:PYFQ5Z*><J07NJ3<b*"
">J[+J+b)2E+52](NO`@ggPQEEY\\\\OOa@egmK*RJ)B*45>*4<:J4N*6Vb*JK[+7*I)BD+U4=*K4"
"NJ\\Zb*I[Z+@VK)*)G5RR6<J*ON2TFbJ65[+(2V)_Q(5Rf6<J*ON@nfcSg9QUL=?M`lfoSo9m5Zg"
"6<J*OV@>gCPgFQSL9?U`<gOPoFm52d6<J*ONRWGb*Z*Y+DXV);G6]lLoO:+VZgeQ[EU54F)<*G46"
"a3dE^]BM[lH?L*>R`*LAZ+YdJ)ZT35P<3@oO:+TRgbKgmJ):Q+5T?"")<*LTNR*W*ge+dU*9M5F*"
"5:?LZ*3ZfnSa)FK+U4=Bd4N*n>c:+4[P]GM5D*7<2H3^@nZ^MZ+7eJ))S+5J99<FS8R`aJ(f[+:G"
"K)*nm5*bm=*ZmU`<gLPm)ZC(5T87<6*7N*)ZbJ+ZZ+VYJ)JJ65f<)<fn5*",
o[256],w[256][64],NN[8],s[5][65536],l[299999],
sn[32]="DrWily Beta by NESMusa";
#include <stdio.h>
#include <unistd.h>
static int ss,m,IN,IV[99],IP[256],PP[100],b,M,I,D[99],Bxx,Cxx,B,Ss,CP,
At[256][64],Np=0,S=SEEK_SET,h,OF,PN,d,p=1,r,k,i,a,c,CO,j,x,T=304,f=8000,LS=0,
P[12],H[99],IL[99];FILE*fo,*fi=stdin;
#define rw(a) a=fgetc(fi),a|=fgetc(fi)<<8
static int Out(int v){int e=0,s=1,f=0;if(OF)v=128+v/30;else for((s=(v<0))?v=-v
:0;e<8;e++)if((f=((v+32)>>(e+1))-16)<16)break;return fputc(OF?v:~((s<<7)|(e<<4
)|f),fo);}void gn(void){for(f=0;A[1]>='0'&&A[1]<='9';f=f*10+*++A-'0');}int
G(int n){while((k=1<<n)>p)d+=p*(by)((*z++^134)+84),p*=64;PN=d&(k-1);d/=k,p/=k;
return PN;}int main(int C,char**V){
if(V)a0=*V;for(P[CO=a=i=C?0:(fseek(fo,0,SEEK_END),r=ftell(fo)/8,rewind(fo),
fread(l,8,r,fo),fclose(fo),remove(tmp),z=(by*)In,main(-1,0))]=907;++i<12;P[i
]=P[i-1]*89/84);if(C<0){if(isatty(fileno(fo=stdout)))fo=fopen(A=OF?"/dev/dsp":
"/dev/audio","wb");if(!fo){perror(A);exit(-1);}a=c=i=sn[29]=0;
for(fprintf(stderr,"Playing %s (%s)...\n",*z-'*'?z:z+1,sn);;
i<3?i++:((c?c--^Out(S/9):++a>=r?a=LS:(c=f*5/T)),i=S=0)){
x=l[a*8+4+i],j=i?i-1:0;H[i] += (D[j]?D[j]*P[x%12]/14500:P[x%12])<<x/12;
if(IL[j])S += (s[j][((unsigned)H[i]/f)%IL[j]]-128)*l[a*8+i];}}
while(--C)if(*(A=*++V)-'-')In=A;else if(A[1])while(*A&&A[1])
*++A=='r'?gn(),0:*A=='d'?OF=1:*A=='h'?h=1:0;else In="*stdin";
if(!In){fprintf(stderr,"%s\n\nUsage:\t %s [options] nesfile.s3m\n"
"Example: %s mman3_d2.s3m\n\t %s -dr22050 cv2b.s3m|esdcat "
"-b -m -r 22050\nOptions:\n"
"\t -d\tPlay in linear format instead & use /dev/dsp if not piped\n"
"\t -r#\tSelect sampling rate in Hz\n\t -h\tThis help\n\n",Title,a0,a0,a0);
if(h)exit(0);for(r=2496,LS=192,c=4;c--;)for(a=0;a<r;)if((S=G(7))-7)
l[c+8*a+4]=S,l[c+8*a++]=G(6)^63;else for(i=G(10),S=G(8);S--;a++)
l[a*8+4+c]=l[(a-i)*8+4+c],l[a*8+c]=l[(a-i)*8+c];
for(i=IL[0]=IL[1]=32;--i>=0;s[0][i]=(i&24)?170:20)s[1][i]=16*(i<16?i:31-i);
for(i=IL[2]=999;--i>=0;s[2][i]=(a=a*999+1)%200);z=(by*)"built-in song";
main(i,0);}if(*In-'*')fi=fopen(In,"rb");if(!fi||!(fo=fopen(tmp,"wb+")))
{perror(fi?tmp:In);return-1;}fread(sn,1,32,fi);rw(m);rw(IN);
rw(PN);fseek(fi,49,S);x=fgetc(fi);T=fgetc(fi)*2;fseek(fi,0x60,S);
fread(o,m,1,fi);for(i=0;i<IN;i++)rw(IP[i]);for(i=0;i<PN;i++)rw(PP[i]);
for(i=0;i<IN;i++)fseek(fi,IP[i]*16+13,S),fgetc(fi),rw(LS),rw(IL[i]),rw(j),
fseek(fi,8,SEEK_CUR),IV[i]=fgetc(fi),fseek(fi,3,SEEK_CUR),
rw(D[i]),fseek(fi,LS*16,S),fread(s[i],1,IL[i],fi);
for(;;){if(CO>=m)main(0,V);if((CP=o[CO])-254){fseek(fi,PP[CP]*16,S);
rw(i);if(!i)PP[CP]=0;if(PP[CP])fread(z=s[4],i-=2,1,fi);for(r=0;r<64;r++){if(!r)ss=1;
a?0:w[CO][r]?(LS=At[CO][r]),main(0,V):(At[CO][r]=ftell(fo)/8,(w[CO][r]=1));
Bxx=Cxx=-1,Ss=ss,j=B=ss=0;if(PP[CP])while((b=*z++)-(M=I=0)){
if(b&32){if(!a){if(*z<254)NN[(b&3)+4]=*z%16+12*(*z/16);else
if(*z==254)NN[b&3]=0;if(z[1]&&*z!=254)NN[b&3]=IV[z[1]-1];}z+=2;}
if(b&64){if(!a)NN[b&3]=*z;z++;}if(b&128){M=*z++;I=*z++;}
if(M==1&&!a)x=I;M==2?Bxx=I:M==3?Cxx=I:M==4?ss=1:0;
if(M==19){if(I/16==11)*((B=I&15)?&ss:&Ss)=1;if(I/16==14)j=I&15;}}if(a)
a--;else{if(Ss)Np=ftell(fo);for(i=(j+1)*x;i;i--)fwrite(NN,1,8,fo);
if(B){c=ftell(fo)-Np;fseek(fo,Np,S);fread(l,1,c,fo);
fseek(fo,Np+c,S);for(i=B;i;i--)fwrite(l,1,c,fo);}
if(Bxx>=0){CO=Bxx-1;if(Cxx<0)break;}
if(Cxx>=0){a=(Cxx&15)+10*(Cxx/16);break;}}}}CO++;}}
