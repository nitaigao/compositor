CC=gcc

compositor: main.c
	$(CC) -o compositor -I/usr/include -I/usr/include/pixman-1 -lwlroots -lwayland-server main.c

clean:
	$(RM) compositor
