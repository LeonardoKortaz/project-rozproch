all:
	gcc -g server.c world.c OpenSimplex/OpenSimplex2F.c player.c -o server
	gcc -g client.c world.c -o client -lSDL2 -lSDL2_ttf

clean:
	rm server client
