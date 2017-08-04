#define _POSIX_SOURCE 1
static char *Title = "NES-S3M player for /dev/audio (8-bit, �-law)\n"
"Copyright (C) 1992,1999 Bisqwit (http://iki.fi/bisqwit/)";
typedef unsigned char by;char*A,*In,*tmp="nesmusa.$$$";
static by*z=(by*)"�\4�\4�\4�\0��������}��=����ͽ��}��=����\14��\37}�/=�?�"
"�O��^}�n=�~��}}�}}�}}�}R�\0�������0����0��������y�0��������}�0�0����������"
"�����}�\14\4�0��������}�0�0�0�������}N�0����0����0�������0�������B��\0������"
"�\24�\20����\24�\20����\24�\20����\24�\20����������������ʽ�ֽ������\6��\22"
"��*��6��B��Z��f��\24�\20�������~��\14����\24�\20�\14����\24�\20�\14����\24�"
"\20�\14�������\24�\20�\14����\24�\20�\14����~��\4�\14�\14�\0�\14�\20�\24�\30"
"�\34� �$�(�,�0�4�8�<�\0����<�<�<�\0�\0�\0�8�8�8�4�4�4�0�0�0�,�,�,�(�(�(�$�$�"
"$� � � �\34�\34�\34�\30�\30�\30�\24�\24�\24�\20�\20�\20�\0����\10����\10����"
"\10����\10����������������ʽ�ֽ������\6��\22��*��6��B��Z��f��\10�������~��t"
"��\10����\10�������~�����~�",o[256],w[256][64],s[5][65536],l[299999],NN[8],
sn[32]="clip from Goonies 2";
#include <stdio.h>
#include <unistd.h>
#define rw(a) a=fgetc(fi),a|=fgetc(fi)<<8
static int ss,S=SEEK_SET,h,m,OF,IN,PN,IP[256],PP[100],r,b,M,I,Bxx,Cxx,B,Ss,
i,a,c,CO,CP,j,x,T=300,f=8000,At[256][64],Np=0,LS=0,P[12],H[99],D[99],IL[99],IV[99];
FILE*fo,*fi=stdin;
static int Out(int v){int e=0,s=1,f;if(OF)v=128+v/26;else for((s=(v<0))?v=-v:
0;e<8;e++)if((f=((v+32)>>(e+1))-16)<16)break;return fputc(OF?v:255^((s<<7)|(e
<<4)|f),fo);}void gn(void){for(f=0;A[1]>='0'&&A[1]<='9';f=f*10+*++A-'0');}
int main(int C,char**V)
{
	for(P[CO=a=i=C?0:(fseek(fo,0,SEEK_END),r=ftell(fo)/8,rewind(fo),fread
		(l,8,r,fo),fclose(fo),remove(tmp),z=(by*)In,main(-1,0))]=907;++i<12;
		P[i]=P[i-1]*89/84);
	if(C<0)
	{	if(isatty(fileno(fo=stdout)))fo=fopen(A="/dev/audio","wb");
		if(!fo){perror(A);exit(-1);}a=c=i=sn[29]=0;
		for(fprintf(stderr,"Playing %s (%s)...\n",*z-'*'?z:z+1,sn);;
		i<3?i++:((c?c--^Out(S/10):++a>=r?a=LS:(c=f*5/T)),i=S=0)) {
			x=l[a*8+4+i],j=i?i-1:0;
			H[i] += (D[j]?D[j]*P[x%12]/14500:P[x%12]) << (x/12);
			if(IL[j])S += (s[j][((unsigned)H[i]/f)%IL[j]]-128)*l[a*8+i];
	}	}
	while(--C)if(*(A=*++V)-'-')In=A;else if(A[1])while(*A&&A[1])
	*++A=='r'?gn(),0:*A=='d'?OF=1:*A=='h'?h=1:0;else In="*stdin";
	if(!In)
	{
		fprintf(stderr,"%s\n\nUsage:\t nesplay [options] nesfile.s3m\n"
		"Example: nesplay cv2b.s3m\n\t nesplay -dr22050 mman3_d2.s3m|esdcat "
		"-b -m -r 22050\nOptions:\n"
			"\t -d\tDon't play in �-law\n"
			"\t -r#\tSelect sampling rate in Hz\n"
			"\t -h\tThis help\n",Title);if(h)exit(0);
		for(r=1280,LS=128,c=4;c--;)for(a=0;a<r;z++)
			if((S=*z++^255)-2)l[c+8*a+4]=S,l[c+8*a++]=*z;else
			for(i=(*z++^130)*256,S=(i+=(*z^130))&63,i/=64;S--;a++)
				l[a*8+4+c]=l[(a-i)*8+4+c],l[a*8+c]=l[(a-i)*8+c];
		for(i=IL[0]=IL[1]=32;--i>=0;s[0][i]=(i&24)?170:20)s[1][i]=i<16?i*16:(31-i)*16;
		for(i=IL[2]=4096;--i>=0;s[2][i]=(a=a*999+1)%200);z=(by*)"built-in song";main(i,0);
	}
	if(*In-'*')fi=fopen(In,"rb");if(!fi||!(fo=fopen(tmp,"wb+")))
	{perror(fi?tmp:In);return-1;}fread(sn,1,32,fi);rw(m);rw(IN);
	rw(PN);fseek(fi,49,S);x=fgetc(fi);T=fgetc(fi)*2;fseek(fi,0x60,S);
	fread(o,m,1,fi);for(i=0;i<IN;i++)rw(IP[i]);for(i=0;i<PN;i++)rw(PP[i]);
	for(i=0;i<IN;i++)
		fseek(fi,IP[i]*16+13,S),fgetc(fi),rw(LS),rw(IL[i]),rw(j),
		fseek(fi,8,SEEK_CUR),IV[i]=fgetc(fi),fseek(fi,3,SEEK_CUR),
		rw(D[i]),fseek(fi,LS*16,S),fread(s[i],1,IL[i],fi);
	for(;;){if(CO>=m)main(0,V);if((CP=o[CO])-254){fseek(fi,PP[CP]*16,S);
	rw(i);if(!i)PP[CP]=0;if(PP[CP])fread(z=s[4],i-=2,1,fi);
	fprintf(stderr, "--Order %02X/%02X, pattern %d--\n",CO,m,CP);
	for(r=0;r<64;r++){if(!r)ss=1;
		fprintf(stderr, "%c%d",a?' ':'|',r);
		a?0:w[CO][r]?(LS=At[CO][r]),main(0,V):(At[CO][r]=ftell(fo)/8,(w[CO][r]=1));
		Bxx=Cxx=-1,Ss=ss,j=B=ss=0;
		if(PP[CP])while((b=*z++)-(M=I=0))
		{	fprintf(stderr, "\r\33[%dC", 4+13*(b&31));
			if(b&32){fprintf(stderr,"%02X %02X",*z,z[1]);if(!a){if(*z<254)NN[(b&3)+4]=*z%16+12*(*z/16);else
			if(*z==254)NN[b&3]=0;if(z[1]&&*z!=254)NN[b&3]=IV[z[1]-1];}z+=2;}else fprintf(stderr, ".. ..");
			if(b&64){fprintf(stderr," %02X",*z);if(!a)NN[b&3]=*z;z++;}else fprintf(stderr, " ..");
			if(b&128){M=*z++;I=*z++;}
			fprintf(stderr, " %c%02X|",M?M+'@':'.', I);
			if(M==1&&!a)x=I;M==2?Bxx=I:M==3?Cxx=I:M==4?ss=1:0;
			if(M==19)
			{	if(I/16==11)*((B=I&15)?&ss:&Ss)=1;
				if(I/16==14)j=I&15;
			}
			fflush(stderr);
		}
		if(a)a--;else
		{	if(Ss)Np=ftell(fo);for(i=(j+1)*x;i;i--)fwrite(NN,1,8,fo);
			if(B){c=ftell(fo)-Np;fseek(fo,Np,S);fread(l,1,c,fo);
			fseek(fo,Np+c,S);for(i=B;i;i--)fwrite(l,1,c,fo);}
			if(Bxx>=0){CO=Bxx-1;if(Cxx<0)break;}
			if(Cxx>=0){a=(Cxx&15)+10*(Cxx/16);break;}
		}
		fprintf(stderr,"\n");
}}CO++;}}
