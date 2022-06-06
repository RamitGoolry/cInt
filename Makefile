default: compile

compile:
	gcc -Wall -g -o interpreter interpreter.c

test:
	./interpreter test.c
