all: warp

warp: warp.c
	gcc -Wall -O2 -o warp warp.c

clean:
	rm *.o
