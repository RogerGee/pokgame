@echo off

REM make-test.bat - pokgame
REM This batch file takes one argument, the test .c file
REM to use. It will be linked against the pokgame code
REM base. The POKGAME_TEST flag will be turned on which
REM will disable the program's "main" function in favor
REM of the one supplied in the test module.

if '%1'=='' echo Specify a test .c file on command line
if '%1'=='' goto end

if not exist Release md Release
set POK=%CD%
set SRC=%CD%\src
cd Release

cl /Fepokgame-test.exe ^
    /D POKGAME_WIN32 /D POKGAME_OPENGL /D POKGAME_TEST /D inline="" ^
    ^ /I %SRC% ^
    %POK%\%1 ^
    %SRC%\character-context.c ^
    %SRC%\character.c ^
    %SRC%\effect.c ^
    %SRC%\error.c ^
    %SRC%\graphics.c ^
    %SRC%\image.c ^
    %SRC%\map-context.c ^
    %SRC%\map.c ^
    %SRC%\net.c ^
    %SRC%\netobj.c ^
    %SRC%\parser.c ^
    %SRC%\pok-util.c ^
    %SRC%\pokgame.c ^
    %SRC%\spriteman.c ^
    %SRC%\tile.c ^
    %SRC%\tileman.c ^
    %SRC%\types.c ^
    %SRC%\update-proc.c ^
    Kernel32.lib ^
    User32.lib ^
    GDI32.lib ^
    OpenGL32.lib ^
    WS2_32.lib ^
    msvcrt.lib ^
    dstructs.lib ^
    /link ^
    /SUBSYSTEM:WINDOWS ^
    /ENTRY:main ^
    /NODEFAULTLIB

set SRC=
set POK=
cd ..

:end
