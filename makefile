clipthumb: clipthumb.o
	$(CC) -o clipthumb clipthumb.o -lsqlite3
clipthumb.o: clipthumb.c
	$(CC) -c -o clipthumb.o clipthumb.c
clean:
	rm -f clipthumb.o clipthumb
