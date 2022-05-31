default: compile

compile:
	gcc -Wall -g -o interpreter interpreter.c

hello:
	./interpreter hello.c
