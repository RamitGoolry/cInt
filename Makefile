default: compile

compile:
	gcc -Wall -o interpreter interpreter.c

hello:
	./interpreter hello.c
