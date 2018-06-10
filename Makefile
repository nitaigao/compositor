CC=gcc

compositor: main.c
	$(CC) -g -o compositor -I. -I/usr/include -I/usr/include/pixman-1 -lxkbcommon -lwlroots -lwayland-server main.c

clean:
	$(RM) compositor
