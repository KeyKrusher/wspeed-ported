@echo off
FIND "intgen" < makefile | FIND /v "DEFdep" | FIND /v "#" > dointg1.bat
call dointg1.bat
del dointg1.bat
del nesmusa.$$$
