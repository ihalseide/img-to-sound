tool: img_to_sound.c stb_image.h
	cc -Wall -O3 -o tool img_to_sound.c -lm

debug: img_to_sound.c stb_image.h
	cc -Wall -DDEBUG -g -o debug img_to_sound.c -lm
