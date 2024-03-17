all:
	gcc -g server.c -o server
	gcc -g client.c -o client

format:
	clang-format -style=file -i server.c client.c
