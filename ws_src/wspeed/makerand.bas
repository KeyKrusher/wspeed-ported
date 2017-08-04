OPEN "wspeed.dan" FOR OUTPUT AS #1
PRINT #1, "Random words (a..z)"

n$ = "abcdefghijklmnopqrstuvwxyz"

lim = 1000
GOSUB MakeRand

OPEN "wspeed.dau" FOR OUTPUT AS #1
PRINT #1, "Random words with Swedish umlauts"

n$ = "abcdefghijklmnopqrstuvwxyzÜÑî"

lim = 750
GOSUB MakeRand

OPEN "wspeed.daf" FOR OUTPUT AS #1
PRINT #1, "Random words with numbers and most umlauts"

n$ = "abcdefghijklmnopqrstuvwxyz0123456789†Ç°¢£ÖäçïóÑâãîÅ§Éàåìñá"

lim = 500
GOSUB MakeRand

END

MakeRand:

    FOR a = 1 TO lim
        b = 3 + INT(RND * 5)

        s$ = ""
        FOR c = 1 TO b
            s$ = s$ + MID$(n$, 1 + INT(RND * LEN(n$)), 1)
        NEXT
        PRINT #1, s$
    NEXT
    CLOSE #1
RETURN

