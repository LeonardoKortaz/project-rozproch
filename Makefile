all:
	gcc -g server.c world.c player.c -o server
	gcc -g client.c world.c -o client -lSDL2

clean:
	rm server client
