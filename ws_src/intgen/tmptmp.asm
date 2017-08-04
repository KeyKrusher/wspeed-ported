.386c
jumps
code segment byte use16
org 100h
assume cs:code,ds:code,es:nothing,ss:code
start:call CPU
Msg	db 'This executable has been assembled from a '
	db 'file created with intgen.',13,10,'Playing "'
	db "TopMan by NESMusa",0
Ms2 db '"...',13,10,0
cld
mov ax,cs
mov ds,ax
mov es,ax
mov ah,4Ah
mov bx,offset Loppu+31
int 21h
call Tu
in al,dx
mov si,ax
mov ax,0FF02h
call OB
mov ax,2104h
call OB
xor cx,cx
@@D:loop @@D
in al,dx
mov di,ax
call Tu
and si,224
jnz N
and di,224
cmp di,192
je Do
N:lea edx,CT
mov[edx],' LPO'
Q:call F9
mov ax,4C01h
int 21h
Tu:mov ax,6004h
call OB
mov ax,8004h
jmp OB
Do:call ID
call RS
call @@A
mov L.word ptr 2,ax
call @@A
mov L.word ptr 6,ax
call @@A
mov L.word ptr 10,ax
call @@A
mov L.word ptr 14,ax
mov bp,3
@@P3:xor dx,dx
mov bx,dx
mov si,bp
add si,si
add si,bp
les di,dword ptr L+bp+si
@@P4:call @@R
not al
xchg cx,ax
call @@R
cmp cl,2
je @@P1
mov ah,cl
mov es:[bx+di],ax
db 'BCC'
jmp @@P2
@@P1:xor al,240
xchg cx,ax
call @@R
xor al,240
mov ah,cl
shr ah,6
and cx,63
add ax,ax
@@L:mov si,di
sub si,ax
mov si,es:[bx+si]
mov es:[bx+di],si
db 'BCC'
loop @@L
@@P2:cmp dx,3042
jb @@P4
dec bp
jns @@P3
lea edx,Msg
call F9
lea edx,Ms2
call F9
Replay:call Delay
les si,L.dword ptr 0
lea edi,S1
mov bp,0
mov si,0
call @@W
les si,L.dword ptr 4
lea edi,S1
mov bp,1
mov si,1
call @@W
les si,L.dword ptr 8
lea edi,S2
mov bp,2
mov si,2
call @@W
les si,L.dword ptr 12
lea edi,S3
mov bp,8
mov si,3
call @@W
db 184
r dw 0
inc ax
cmp ax,3042
jb @@LO
mov ax,321
@@LO:mov r,ax
mov ah,11h
int 16h
jz Replay
call RS
mov ax,4C00h
int 21h
@@R:mov si,Data
xor ax,ax
lodsb
mov Data,si
ret
@@W:mov bx,r
add bx,bx
mov al,es:[bx]
or al,al
jnz @@w1
lea ax,[si+176]
call OB
ret
@@w1:push si
lea ax,[bp+32]
mov ah,[di+0]
call OB
lea ax,[bp+35]
mov ah,[di+1]
call OB
lea ax,[bp+61536]
call OB
lea ax,[bp+61539]
call OB
lea ax,[bp+61568]
call OB
lea ax,[bp+61571]
call OB
lea ax,[bp+224]
mov ah,[di+4]
call OB
lea ax,[bp+227]
mov ah,[di+5]
call OB
mov si,64
mov dl,[di+2]
call VL
mov si,67
mov dl,[di+3]
call VL
pop bp
lea ax,[bp+192]
mov ah,[di+6]
call OB
mov al,es:[bx+1]
aam 12
movzx cx,ah
mov si,ax
and si,15
add si,si
mov ax,P[si]
jcxz @@f3
@@f:add ax,ax
loop @@f
@@f3:cmp ax,512
jb @@f2
inc cx
shr ax,1
jmp @@f3
@@f2:
xchg bx,ax
and cx,7
and bh,3
lea ax,[bp+160]
mov ah,bl
call OB
lea ax,[bp+176]
mov ah,cl
or ah,8
shl ah,2
or ah,bh
call OB
ret
VL:mov al,dl
and al,63
sub al,63
mov cl,63
neg al
mul es:byte ptr[bx]
shr ax,6
sub cl,al
and dl,192
or cl,dl
lea ax,[bp+si]
mov ah,cl
jmp OB
Data dw Musa
Musa db 254,0,'˝œÒ˝œ∞˝œè˝œN˝Ï',13,'”<”8”3”.”+˝”ıÿ<ÿ8ÿ3ÿ+ÿ%Ÿ%Ÿ%Ÿ',0,'˝¸Ò”<”'
     db '8”3”+”%’%’%’',0,'˝¸Ò˝˙ÿ˝˙¬˝¬ÿ˝‰û˝ÿ†˝œP˝œP˝œP˝è∞˝ì∞Ÿ%˝πH˝ΩI˝˙Ó˝°',16
     db '˝Áÿ˝Ó†˝˙î˝ÎÓ˝} ˝zæ˝‰˙˝Op˝_¿˝Op˝',15,'–˝œP˝',15,'–˝è∞˝Op˝Op˝m ˝',15
     db '1˝',15,'1˝Oq˝Oq˝',15,'—˝',15,'—˝',15,'1˝',15,'1˝',15,'1˝',15,'1˝'
     db 15,'1˝',15,'1˝<1˝',27,'Å˝.',127,'˝‰˙”=”9”5”.”+’+˝˜Ò”+˝ÙÒ—+—+—+—+œ+'
     db '˝˜Òœ',0,'˝œÒ˝‘∞˝˜r’',0,'˝˘|˝‰˙˝œP˝¬8˝4',12,211,0,'˝”ı˝',15,'0˝',31
     db 24,'˝$ñ˝',15,'0˝_p˝dú˝Ã‰‰=˝˜Ò‰',0,228,0,228,0,'Â=˝˜ÒÂ',0,229,0,'Á='
     db '˝˜ÒÁ',0,231,0,'‚=˝ÁÒ‚',0,'˝ıÒ˝˙Õ˝¬Ã·=˝˜Ò·',0,225,0,'‡=˝˜Ò‡',0,224
     db 0,'˝˙àÈ=˝˜ÒÈ',0,233,0,'˝Ãî˝œP˝·P˝∫›„=˝˜Ò„',0,227,0,'˝∫‹˝œ˙˝œ∂˝œr˝œ'
     db 'N˝äÙ˝∫',26,'˝œ˙˝œ∂˝œr˝√NË=˝˜ÒË',0,'˝˚Ò˝:ä˝˙Ú˝.ñ˝Op˝OL˝O',8,253,3,'ƒ'
     db '˝:',18,'˝œ˙˝7',28,253,15,'0˝',15,12,253,15,12,'˝è∞˝∏∫˝è±˝O{˝O7˝',15
     db 'Û˝Oq˝O{˝Oq˝',15,13,253,15,13,253,15,'1˝',15,'1˝',15,'1˝',15,3,'˝&'
     db 13,'Ë=˝˜ÒË',0,232,0,'È=˝œÒ˝¸∞È',0,'˝œÒ˝˙∏Í=˝˜ÒÍ',0,234,0,'Ï=˝⁄ÒÏ',0
     db '˝œÒ˝—∞˝˙ôÌ=˝˜ÒÌ',0,237,0,'Ó=˝˜ÒÓ',0,238,0,'˝œ˙˝œ∂˝“r˝∫V˝œ˙˝°',28,'Î'
     db '=˝˜ÒÎ',0,'˝˚Ò˝∫2˝˙Ú˝dÿ˝zzÿ9ÿ3˝˜Òÿ1ÿ.Ÿ9Ÿ3˝ˆÒŸ1Ÿ.€9€3˝ˆÒ€1€.÷9÷3˝ËÒ'
     db '÷1÷.÷+÷+˝¯Õ˝ƒÃ’9’3˝ˆÒ’1’.‘9‘3˝ˆÒ‘1‘.˝˙à›9›3˝ˆÒ›1›.˝Ãî˝œP˝·P˝˙',0,'◊'
     db '9◊3˝ˆÒ◊1◊.˝˙N˝˙8œ9œ3˝ˆÒœ1œ.˝˙,—9—3˝ˆÒ—1—.˝˙',0,'”9”3˝ˆÒ”1”.˝∫Ù˝∏Ç'
     db '÷1÷.˝ÓÃ˝÷î˝˙˜˝œP˝œP˝†Ù˝b®÷1÷.˝˙',10,'˝‚ÿ÷1÷.˝∫‹˝§“˝∫∆˝zd˝Óÿ˝÷†˝œP'
     db '˝£&˝z*‹9‹3˝ˆÒ‹1‹.‹+‹+‹%‹',0,'˝ˆÒ˝:ä€+€+€%€',0,'˝ˆÒ˝,ñ˝˙˜€1€.˝:F˝$'
     db '¢˝z®˝:.˝:0˝œ†˝œ†˝⁄P˝¸',22,'˝œ',0,'˝Î† 9 3˝ˆÒ 1 . + + % ',0,'˝ˆÒ˝¯'
     db '‰˝ÙÎÀ3À3À3Ã3˝˚ÒÃ1Ã.Ã+˝8',2,'˝˙˜œ1œ.˝:R˝ﬂ†˝˚®Ã+Ã%˝œ',0,'˝ï∞€+˝èa˝è'
     db '±˝Ù',24,'˝è±˝•±˝µ∞Ã',0,'˝O¡˝Oq˝Oq˝è∞˝è∞˝è∞˝',15,'1˝',15,'1˝Op˝',29
     db '1˝8/‹9‹3˝ˆÒ‹1‹.›9›3˝ËÒ›1›1›.›.”9”3˝ˆÒ”1”1”.”.”.”+”+”+”%”%”',0,211
     db 0,'˝˙‰˝ÓÓ˝÷‰˝¯˜˝Ùí˝¯|›1›1ﬁ9ﬁ3˝ˆÒﬁ1ﬁ1‡9‡3˝ËÒ‡1‡1‡.‡.˝œP˝ﬂP˝¯|‡1‡1·9'
     db '·3˝ˆÒ·1·1‚9‚3˝ˆÒ‚1‚1‚',0,226,0,'ÿ+ÿ5˝ÌÒ˝5Qÿ1ÿ1ÿ',0,216,0,'Ÿ+Ÿ5˝ÌÒ'
     db 'Ÿ3Ÿ3Ÿ3Ÿ3Ÿ1Ÿ1Ÿ1Ÿ',0,217,0,'˝ÿ†÷+÷5˝ÌÒ˝5-÷1÷1÷',0,214,0,'‘+‘5˝ÌÒ˝5Ÿ'
     db '‘1‘1‘',0,212,0,'”+”5˝ÌÒ˝v·”1”3”.”',0,'˝˜Ò”:˝˜Ò”6”3”+˝¯„”',0,'“:˝˜'
     db 'Ò“6“3“+“',0,'˝¯Ò—:˝˜Ò—6—3—+—',0,'˝¯Ò–:˝˜Ò–6–3Ã9Ã3˝˜ÒÃ1Ã.Õ9Õ3˝ˆÒÕ1'
     db 'Õ.œ9œ3˝ˆÒœ1œ. 9 3˝ËÒ 1 . + +˝¯Õ˝ƒÃ…9…3˝ˆÒ…1….»9»3˝ˆÒ»1».˝˙à—9—3˝ˆ'
     db 'Ò—1—.˝Ãî˝œP˝·P˝˙',0,'À9À3˝ˆÒÀ1À.˝∫‹˝∫∆˝‰"˝∏∫ 1 .˝∫Æ˝∫∞˝‰',10,'˝Óÿ'
     db '˝÷†˝˙˜˝œP˝œP˝‡',0,'˝∏',16,'˝˙˜—1—.˝∫',4,'˝∫‹˝d¯˝zÏ˝zB˝Ó†˝‚ÿ˝Œ†˝ P'
     db '˝¸',22,'–9–3˝ˆÒ–1–.–+–+–%–',0,'˝ˆÒ˝:äœ+œ+œ%œ',0,'˝ˆÒ˝,ñ˝è',16,'˝O'
     db '¿˝T œ:œ1œ',0,'œ1œ3˝ÙÙœ3˝ıÙœ.˝ÙÙÕ:Õ1Õ',0,'Õ1Õ3˝ÙÙÕ3˝ıÙÕ.˝ÙÙÃ:Ã1Ã',0
     db 'Ã1Ã3˝ÙÙÃ3˝ıÙÃ.˝ÙÙ≈:≈1≈',0,'≈1≈3˝ÙÙ≈3˝ıÙ≈.˝ÙÙ∆:∆1∆',0,'∆1∆3˝ÙÙ∆3˝ı'
     db 'Ù∆.˝ÙÙ : 1 ',0,' 1 3˝ÙÙ 3˝ıÙ .˝ÙÙ˝‰‰∆:∆5∆3∆3∆3∆3≈3≈3≈3≈3∆3«3˝4%«3'
     db '«3«3˝ı¯˝ˆ˚˝‰˙˝˜Ô˝·Ÿ»1»1»1».«.«.»:…5˝5%…3…3…1 :…5˝Œ†˝‚â˝Ùí».».«+«+'
     db '˝è∞˝°∞√:√1√',0,'√1√3˝ÙÙ√3˝ıÙ√.˝ÙÙ√1˝°ô≈1˝£ô∆1∆',0,'∆. : 5˝˜',1,'…'
     db '3 3…3˝ç±˝ı.«.«+˝∏±…1˝è±˝£4«1˝µ∞»+˝è±˝Oq˝Oq˝Oq˝Oq˝Oq˝\q˝',25,25,'˝'
     db 'Op˝',15,'1˝',5,'1«',0,'»%»%»%»',0,253,255,'Ò˝9]Ã1Ã',0,'Ã1Ã.Ã.Ã',0
     db 'Ã+Ã+Ã+Ã',0,'Ã%˝˙‰˝9/Õ1Õ',0,'Õ1Õ.Õ.Õ',0,'Õ+Õ+Õ+Õ',0,'Õ%˝Ó¬˝˙∂˝d*˝'
     db 255,'¯Õ+˝˝ÙÕ1Õ',0,'Õ+Õ.Õ.Õ',0,'Õ%Õ+Õ+Õ%Õ%Õ%Õ',0,'˝¯Ò˝œP˝œP˝˛Pœ:œ5˝'
     db '˜Òœ3œ1œ.œ.œ+œ%œ',0,'˝ÙÒ˝˘‰˝‚¯œ3œ1œ.—:—5˝˜Ò—3˝˙ÃÃ:Ã5˝·ÒÃ3˝÷ÿÃ3Ã1˝Ì'
     db '†Ã3˝œP˝ÎP”:”5˝˜Ò”3”1”.”.”+”%”',0,'˝ÙÒ“:“5˝˜Ò“3“1“.“.“+“%“',0,'˝ÙÒ'
     db '˝˙',0,'—1—.—.—+—%—',0,'˝ÙÒ–:–5˝˜Ò–3'

DC	dd 0
D55	dd 55
S40	dw 40h
L	dd 0,0,0,0
CT	db '386 not found.',13,10,0
S1	db 33, 1, 14,8,   3,0,2
S2	db 34,33,192,192, 0,0,5
S3	db 15, 0,  0,0,   0,1,14
P	dw 411,435,460,487,517,547,580,614,653,691,732,776
Delay:mov cx,12
mov es,S40
mov di,0
mov bl,es:[di]
DC1:mov eax,DC
call DLo
loop DC1
ret
DLo:sub eax,1
jc @q
cmp bl,es:[di]
je DLo
ret
ID:mov es,S40
mov di,6Ch
mov bl,es:[di]
ID2:cmp bl,es:[di]
je ID2
ID3:mov bl,es:[di]
mov eax,-28
call DLo
not eax
cdq
idiv D55
mov DC,eax
ret
F9:mov bx,dx
mov dl,[bx]
or dl,dl
jz @q
mov ah,2
int 21h
inc bx
jmp F9+2
@@A:
mov ah,48h
mov bx,396
int 21h
ret
OB:push cx
mov dx,388h
out dx,al
mov cx,6
o1:in al,dx
loop o1
inc dx
mov al,ah
out dx,al
mov cx,35
o2:in al,dx
loop o2
dec dx
pop cx
ret
CI:xor cx,cx
mov bp,sp
add word ptr[bp],3
iret
CPU:push sp
pop bp
cmp sp,bp
jne CF
mov es,r
push word ptr es:26
push word ptr es:24
mov es:word ptr 24,offset CI
mov es:word ptr 26,cs
mov cx,1
db 15,32,194
jcxz CF
pop dword ptr es:24
push offset S[9]
S:mov bx,[bp]
cmp byte ptr[bx-1],0
je @q
inc word ptr[bp]
jmp S
CF:mov dx,offset CT
jmp Q
RS:xor ax,ax
push ax
call OB
pop ax
inc ax
cmp ax,244
jbe RS+2
@q:ret
Loppu label byte
code ends
end start
