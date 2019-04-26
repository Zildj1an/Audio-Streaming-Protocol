all:	clean server client

%.o:	%.c
	gcc -Wall -O3 -s -std=gnu90 -c -g $<

server:	server.o asp.o
	gcc -Wall -O3 -g  -s -o server server.o asp.o -lm

client:	client.o asp.o
	gcc -Wall -O3 -g  -s -o client client.o asp.o -lm -lasound

clean:
	rm -f server
	rm -f client
	rm -f *.o
