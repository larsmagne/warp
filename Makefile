all: warp

warp: warp.c
	gcc -Wall -O2 -o warp warp.c `pkg-config --cflags --libs glib-2.0` `pkg-config --cflags --libs gmime-2.6`

clean:
	rm *.o
