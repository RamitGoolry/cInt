#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define int long long

int token;
char *src, *old_src;
long long poolsize;
int line;

int *text,      // Text segment
	*old_text;  // for dump text segment
int * stack;    // Stack
char *data;     // Data segment

// Virtual registers
int *pc, *bp, *sp, ax, cycle;

// Instruction set
enum {  LEA, IMM, JMP, CALL, JZ, JNZ, ENT, ADJ, LEV, LI, LC, SI, SC, PUSH,
		OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
		OPEN, READ, CLOS, PRTF, MALC, MSET, MCMP, EXIT };

void next() {
	token = *src++;
}

void expression(int level) {
	// do nothing for now
}

void program() {
	next();

	while (token > 0) {
		/*printf("%c", (char) token);*/
		next();
	}
}

int eval() {
	int op, *tmp;

	while(1) {
		op = *pc++;

		// Memory Ops

		if (op == IMM) {          // Put an Immediate value into register AX
			ax = *pc++;
		}
		else if (op == LC) {    // Load a character into AX from a memory address stored at AX
			ax = *(char *) ax;
		}
		else if (op == LI) {    // Load an int into AX from a memory address stored at AX
			ax = *(int *) ax;
		}
		else if (op == SC) {    // Store the character held at AX to a memory address at the top of the stack
			*(char *)*sp++ = ax;
		}
		else if (op == SI) {
			*(int *)*sp++ = ax;
		}

		// Stack Ops

		else if (op == PUSH) {  // Push the value of AX onto the stack
			*--sp = ax;
		}

		// Jump Ops

		else if (op == JMP) {   // Jump
			pc = (int *) *pc;
		}
		else if (op == JZ) {    // Jump if ax is zero
			pc = ax ? pc + 1 : (int *)*pc;
		}
		else if (op == JNZ) {   // Jump if ax is not zero
			pc = ax ? (int *)*pc : pc + 1;
		}

		// Function call related Ops

		else if (op == CALL) {  // Call a subroutine
			*--sp = (int) pc + 1;
			pc = (int *) *pc;
		}
		// else if (op == RET) {
			// pc = (int *) *sp++;
		//}
		else if (op == ENT) { // Make a new stack frame
			// Will store the current PC value onto the stack, and
			// save some space <size> bytes for local variables
			*--sp = (int) bp;
			bp = sp;
			sp -= *pc++;
		}
		else if (op == ADJ) { // Adjust the stack pointer
			// Remove arguments from frame
			sp += *pc++;
		}
		else if (op == LEV) { // Restore call frame and PC
			sp = bp;
			bp = (int *) *sp++; // Base pointer stored on stack
			pc = (int *) *sp++; // Program counter stored on stack
		}
		else if (op == LEA) { // Load effective address
			ax = (int) (bp + *pc++);
		}

		// Math Ops

		else if (op == OR) {
			ax = *sp++ | ax;
		}
		else if (op == XOR) {
			ax = *sp++ * ax;
		}
		else if (op == AND) {
			ax = *sp++ & ax;
		}
		else if (op == EQ) {
			ax = *sp++ == ax;
		}
		else if (op == NE) {
			ax = *sp++ != ax;
		}
		else if (op == LT) {
			ax = *sp++ < ax;
		}
		else if (op == GT) {
			ax = *sp++ > ax;
		}
		else if (op == LE) { 
			ax = *sp++ <= ax;
		}
		else if (op == GE) {
			ax = *sp++ >= ax;
		}
		else if (op == SHL) {
			ax = *sp++ << ax;
		}
		else if (op == SHR) {
			ax = *sp++ >> ax;
		}
		else if (op == ADD) {
			ax = *sp++ + ax;
		}
		else if (op == SUB) {
			ax = *sp++ - ax;
		}
		else if (op == MUL) {
			ax = *sp++ * ax;
		}
		else if (op == DIV) {
			ax = *sp++ / ax;
		}
		else if (op == MOD) {
			ax = *sp++ % ax;
		}

		// Built in Instructions
		
		else if (op == EXIT) {  // Exit the program
			printf("exit(%lld)", *sp);
			return *sp;
		}
		else if (op == OPEN) { // Open File
			ax = open((char *) sp[1], sp[0]);
		}
		else if (op == CLOS) { // Close file
			ax = close(sp[0]);
		}
		else if (op == READ) { // Read from file
			ax = read(sp[2], (char *) sp[1], sp[0]);
		}
		else if (op == PRTF) { // Printf
			tmp = sp + pc[1];
			ax = printf((char *) tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);
		}
		else if (op == MALC) { // Malloc
			ax = (int) malloc(*sp);
		}
		else if (op == MSET) { // Memset
			ax = (int) memset((char *) sp[2], sp[1], sp[0]);
		}
		else if (op == MCMP) { // Memcmp
			ax = memcmp((char *) sp[2], (char *) sp[1], sp[0]);
		}

		else {
			printf("Unknown opcode: %lld\n", op);
			return -1;
		}
	}

	return 0;
}

int32_t main(int32_t argc, char **argv) {
	int i, fd;

	argc--;
	argv++;

	poolsize = 1024 * 1024; // arbirary size
	line = 1;

	if((fd = open(*argv, 0)) < 0) {
		printf("could not open(%s)\n", *argv);
		return 1;
	}

	if(!(src = old_src = malloc(poolsize))) {
		printf("could not malloc(%lld) for source area\n", poolsize);
		return 1;
	}

	if((i = read(fd, src, poolsize - 1)) <= 0) {
		printf("read() returned %lld\n", i);
		return 1;
	}

	src[i] = 0;
	close(fd);

	// Allocate memory for text, stack, and data
	if (!(text = old_text = malloc(poolsize))) {
		printf("could not malloc(%lld) for text area\n", poolsize);
		return 1;
	}

	if(!(data = malloc(poolsize))) {
		printf("could not malloc(%lld) for data area\n", poolsize);
		return 1;
	}

	if(!(stack = malloc(poolsize))) {
		printf("could not malloc(%lld) for stack area\n", poolsize);
		return 1;
	}

	memset(text, 0, poolsize);
	memset(data, 0, poolsize);
	memset(stack, 0, poolsize);

	bp = sp = (int *) ((int) stack + poolsize);
	ax = 0;

	i = 0;
    text[i++] = IMM;
    text[i++] = 10;
    text[i++] = PUSH;
    text[i++] = IMM;
    text[i++] = 20;
    text[i++] = ADD;
    text[i++] = PUSH;
    text[i++] = EXIT;
    pc = text;

	program();
	return eval();
}
