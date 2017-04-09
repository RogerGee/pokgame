@echo off

REM build.bat - pokgame
REM This batch script builds pokgame for MS Windows targeting MinGW. Make
REM sure you have the following dependencies:
REM	libdstructs.a
REM	libpng.a
REM Currently this script generates a debug build.

gcc -s -o pokgame-test -std=c99 -Wfatal-errors ^
	-Wno-pointer-to-int-cast ^
	-I. ^
	-Isrc ^
	-D POKGAME_WIN32 -D _WIN32_IE=0x0500 -D POKGAME_TEST -D POKGAME_DEBUG ^
	test\main.c ^
	test\maintest.c ^
	test\nettest.c ^
	test\graphicstest1.c ^
	src\character-context.c ^
	src\character.c ^
	src\config-win32.c ^
	src\default.c ^
	src\effect.c ^
	src\error.c ^
	src\gamelock.c ^
	src\graphics-win32.c ^
	src\graphics.c ^
	src\image.c ^
	src\io-proc.c ^
	src\map-context.c ^
	src\map.c ^
	src\menu.c ^
	src\net.c ^
	src\netobj.c ^
	src\parser.c ^
	src\pok-util.c ^
	src\pokgame.c ^
	src\primatives.c ^
	src\spriteman.c ^
	src\standard1.c ^
	src\tile.c ^
	src\tileman.c ^
	src\types.c ^
	src\update-proc.c ^
	src\user.c ^
	-lopengl32 -lws2_32 -lgdi32 -ldstructs -lpng
