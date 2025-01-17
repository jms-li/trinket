#!/bin/sh

OS=$(uname)

case $OS in
	Linux)
		gcc -Wall -Wextra -I. -DSDL_PLATFORM -o ./bin/trinket trinket.c -lm -lSDL2 && ./bin/trinket
		;;
	Darwin)
		gcc -I/Users/lij/tools/SDL2/SDL-release-2.30.9/include -Lsrc/lib -o ./bin/trinket trinket.c -lSDL2main -lSDL2 -framework CoreVideo -framework Cocoa -framework IOKit -framework CoreAudio -framework Metal -framework AudioToolbox -framework CoreHaptics -framework GameController -framework ForceFeedback -framework Carbon -framework QuartzCore -framework AppKit -framework CoreFoundation -framework CoreGraphics -framework CoreServices -framework Foundation && ./bin/trinket
		;;
esac

