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

// Tokens and classes for lexical analysis
enum {
	Num = 128, Fun, Sys, Glo, Loc, Id,
	Char, Else, Enum, If, Int, Return, Sizeof, While,
	Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr,
	Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

int token_val;         // Value of current token
int *current_id,       // current parsed ID
	*symbols;          // Symbol table

// fields of identifier
enum {Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize};

void next() {
	char* last_pos;
	int hash;

	while ((token = *src)) {
		++src;
		// parse token
	
		if(token == ' ' || token == '\t'); // Skip whitespaces
		else if(token == '\n') {           // New line
			line++;
		}
		else if(token == '#') {
			// Macros - Maybe TODO in the future, but currently not supported
			while(*src != 0 && *src != '\n')
				++src;
		}
		else if((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || 
			(token == '_')) {
			// Identifier
			
			last_pos = src - 1;
			hash = token;

			while((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') ||
					(*src >= '0' && *src <= '9') || *src == '_') {
				hash = (hash * 147) + *src;
				src++;
			}

			// Look for existing identifier through linear search
			current_id = symbols;

			while(current_id[Token]) {
				if(current_id[Hash] == hash && !memcmp((char *) current_id[Name], last_pos, src - last_pos)) {
					// found identifier, return
					token = current_id[Token];
					return;
				}
				current_id += IdSize;
			}

			// Not found, store new ID
			current_id[Name] = (int) last_pos;
			current_id[Hash] = hash;
			token = current_id[Token] = Id;
			return;
		}
		else if(token >= '0' && token <= '9') {
			// Numeric literal
			
			token_val = token - '0';

			if(token_val > 0) { // Decimal
				while (*src >= '0' && *src <= '9') {
					token_val = (token_val * 10) + (*src - '0');
					src++;
				}
			} else {
				if(*src == 'x' || *src == 'X') { // Hexadecimal
					token = *++src;
					while ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') ||
							(token >= 'A' && token <= 'F')) {
						token_val = (token_val * 16) + (token & 15) + (token >= 'A' ? 9 : 0);
						token = *++src;
					}
				} else { // Octal
					while (*src >= '0' && *src <= '7') {
						token_val = (token_val * 8) + (*src - '0');
						src++;
					}
				}
			}

			token = Num;
			return;
		}
		else if(token == '"' || token == '\'') {
			// String Literal
			// Parse String Literal, currently only supporting \n and \t as escaped character.
			// Store Literal into data
			
			last_pos = data;
			while(*src != 0 && *src != token) {
				token_val = *src++;
				if(token_val == '\\') {
					// Escape sequence
					token_val = *src++;

					if(token_val == 'n') {
						token_val = '\n';
					} else if (token_val == 't') {
						token_val = '\t';
					}
				}
				if(token == '"') {
					*data++ = token_val;
				}
			}

			src++;
			// If it is a single character, return Num token
			if (token == '"') {
				token_val = (int) last_pos;
			} else {
				token = Num;
			}

			return;
		}
		else if (token == '/') {
			// // or /
			if(*src == '/') {
				// Single Line comment, skip to end of line
				while (*src != 0 && *src != '\n')
					++src;
			}
			// TODO support multiline comments
			// else if(*src == '*') {
			// }
			else {
				// Division operator
				token = Div;
				return;
			}
		}
		else if (token == '=') {
			// == or =
			if (*src == '=') {
				// Equality operator
				src++;
				token = Eq;
			} else {
				// Assignment operator
				token = Assign;
			}
			return;
		}
		else if (token == '+') {
			// ++ or +
			if(*src == '+') {
				// Increment operator
				src++;
				token = Inc;
			} else {
				// Addition operator
				token = Add;
			}
			return;
		}
		else if (token == '-') {
			// -- or -
			if (*src == '-') {
				// Decrement operator
				src++;
				token = Dec;
			} else {
				// Subtraction operator
				token = Sub;
			}
			return;
		}
		else if (token == '!') {
			// !=
			if (*src == '=') {
				// Not equal operator
				src++;
				token = Ne;
			}
			// TODO Support logical not
			return;
		}
		else if (token == '<') {
			// <=, << or <
			if (*src == '=') {
				// Less than or equal operator
				src++;
				token = Le;
			} else if (*src == '<') {
				// Left Shift
				src++;
				token = Shl;
			} else {
				// Less than operator
				token = Lt;
			}
			return;
		}
		else if (token == '>') {
			// >=, >> or >
			if(*src == '=') {
				// Greater than or equal operator
				src++;
				token = Ge;
			} else if (*src == '>') {
				// Right Shift
				src++;
				token = Shr;
			} else {
				// Greater than operator
				token = Gt;
			}
			return;
		}
		else if (token == '|') {
			// || or |
			if(*src == '|') {
				// Logical or operator
				src++;
				token = Lor;
			} else {
				// Bitwise or operator
				token = Or;
			}
			return;
		}
		else if (token == '&') {
			// && or *
			if(*src == '&') {
				// Logical and
				src++;
				token = Lan;
			} else {
				// Bitwise and
				token = And;
			}
			return;
		}
		else if (token == '^') {
			token = Xor;
			return;
		}
		else if (token == '%') {
			token = '%';
			return;
		}
		else if (token == '*') {
			token = Mul;
			return;
		}
		else if (token == '[') {
			token = Brak;
			return;
		}
		else if (token == '?') {
			token = Cond;
			return;
		}
		else if (token == '~' || token == ';' || token == '{' || token == '}' || 
				token == '(' || token == ')' || token == ']' || token == ',' ||
				token == ':') {
			// directly return character as token
			return;
		}
	}
	return;
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

// types of variables / functions
enum { CHAR, INT, PTR };
int *idmain; // the main function

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

	src = "char else enum if int return sizeof while open read close "
		"printf malloc memset memcmp exit void main";

	// Add keywords to symbol table
	
	i = Char;
	while (i <= While) {
		next();
		current_id[Token] = i++;
	}

	// add library to symbol table
	i = OPEN;
	while (i <= EXIT) {
		next();
		current_id[Class] = Sys;
		current_id[Type] = INT;
		current_id[Value] = i++;
	}

	next(); current_id[Token] = Char;  // handle void type
	next(); idmain = current_id;       // keep track of main function

	program();
	return eval();
}
