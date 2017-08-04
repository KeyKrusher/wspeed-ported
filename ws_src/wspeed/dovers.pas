{$I+,M 4096,0,63336}
Uses Windos, Strings;

Var
	f3, f2: File;
	TempF: Text;
	FName: Array[0..128]Of Char;
	Eivoimitaan: Boolean;

Procedure Open(s: PChar);
Var
	n: PChar;
	w: Word;
	l: LongInt;
Begin
	FileMode := 0;

	{$I-}
	Assign(f3, s);
	Reset(f3, 1);
	{$I+}
	Eivoimitaan := IOResult <> 0;
	If Eivoimitaan Then Exit;

	{**WriteLn('"',s,'" open');{}

	GetMem(n, 32768);

	Assign(f2, 'dovers.$$$');
	ReWrite(f2, 1);
	{**WriteLn('"','dovers.$$$','" open');{}
	l := FileSize(f3);
	While l > 0 Do
	Begin
		If l > 32768
			Then w := 32768
			Else w := l;
		BlockRead(f3, n^, w);
		BlockWrite(f2, n^, w);
		Dec(l, w)
	End;
	Close(f2);
	Close(f3);
	{**WriteLn('"','dovers.$$$','" closed');{}
	{**WriteLn('"',s,'" closed');{}

	FreeMem(n, 32768);

	Assign(TempF, 'dovers.$$$');
	Reset(TempF);
	{**WriteLn('"','dovers.$$$','" open');{}
	FileMode := 2;
	Reset(f3, 1);
	{**WriteLn('"',s,'" open');{}
End;

Var
	ThisLine: Array[0..511]Of Char;

Procedure Finish;
Var Buf: Array[0..511]Of Char;
Begin
	If Eivoimitaan Then Exit;
	If ThisLine[0] <> #0 Then
	Begin
		strcat(ThisLine, #13#10);
		BlockWrite(f3, ThisLine, strlen(ThisLine));
		ThisLine[0] := #0
	End;
	While Not Eof(TempF)Do
	Begin
		InOutRes := 0;
		ReadLn(TempF, Buf);
		If IOResult <> 0 Then Break;
		strcat(Buf, #13#10);
		BlockWrite(f3, Buf, strlen(Buf))
	End;
	Truncate(f3);
	Close(f3);
	Close(TempF);
	{**WriteLn('"','dovers.$$$','" closed');{}
	{**WriteLn('"','file','" closed'^J){}
End;

Procedure Change(Old, New: PChar);
Var
	t: Array[0..511]Of Char;
	s: PChar;
	n, w: Word;
Begin
	If Eivoimitaan Then Exit;

	n := strlen(Old);

	s := Nil;
	For w := 0 To strlen(ThisLine) - n Do
		If strncmp(ThisLine+w, Old, n)=0 Then
		Begin
			s := ThisLine+w;
			Break
		End;
	If s=Nil Then Exit;
	strcpy(t, s + strlen(Old));
	strcpy(s, New);
	strcat(s, t)
End;

Procedure Find(s: PChar);
Var Buf: Array[0..511]Of Char;
Begin
	If Eivoimitaan Then Exit;
	If ThisLine[0] <> #0 Then
	Begin
		strcat(ThisLine, #13#10);
		BlockWrite(f3, ThisLine, strlen(ThisLine));
		ThisLine[0] := #0
	End;
	While Not Eof(TempF)Do
	Begin
		{$I-}
		ReadLn(TempF, Buf);
		{$I+}
		If IOResult <> 0 Then Break;
		If strpos(Buf, s)<>Nil Then
		Begin
			strcpy(ThisLine, Buf);
			Break
		End;
		strcat(Buf, #13#10);
		BlockWrite(f3, Buf, strlen(Buf))
	End
End;

Var
	Buf, Buf2: Array[0..128]Of Char;
	f: Text;

Begin
	GetArgStr(Buf, 1, 127);
	If(GetArgCount <> 1)Or(Buf[0]='-')Or(Buf[0]='/')Then
	Begin
		WriteLn('Usage: See versio.inf');
		Exit
	End;
	Assign(f, Buf);
	Reset(f);

	While Not Eof(f)Do
	Begin
		{$I-}
		ReadLn(f, Buf);
		{$I+}
		if IOResult <> 0 Then Break;

		Case Buf[0]Of
			'-': Finish;
			'>':
			Begin
				Find(Buf+1);
			End;
			'<':
			Begin
				ReadLn(f, Buf);
				ReadLn(f, Buf2);
				Change(Buf, Buf2)
			End;
			Else
				Write('Doing...');
				strcpy(FName, Buf);
				Open(Buf)
		End
	End;

	Close(f);

	Assign(f, 'dovers.$$$');
	Erase(f);

	WriteLn('Done')
End.
