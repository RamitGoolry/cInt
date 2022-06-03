#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define int long long

int debug = 1;

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

// types of variables / functions
enum { CHAR, INT, PTR };
int *idmain; // the main function

int basetype;          // The type of a declaration, global for convenience
int expr_type;         // The type of an expression

int index_of_bp;

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

void match(int tk) {
	if (token == tk) {
		next();
	} else {
		printf("Error at line %lld : Expected token %lld, got %lld\n", line, tk, token);
		exit(-1);
	}
}

void enum_declaration() {
	// parse enum [id] {a = 1, b = 2, c = 3, ...}
	
	int i;
	i = 0;

	while (token != '}') {
		if (token != Id) {
			printf("Error at line %lld: Bad enum identifier\n", line);
			exit(-1);
		}
		next();

		if(token == Assign) {
			next();

			if (token != Num) {
				printf("Error at line %lld: Bad enum initialization\n", line);
				exit(-1);
			}

			i = token_val;
			next();
		}

		current_id[Class] = Num;
		current_id[Type] = INT;
		current_id[Value] = i++;

		if (token == ',') {
			next();
		}
	}
}

void expression(int level) {
	// expressions have various formats
	// can be divided mostly into 2 parts : unit and operator
	//
	// We will define expressions as:
	//
	// 1. unit_unary ::= unit | unit unary_op | unary_op unit
	// 2. expr ::= unit unary (bin_op unit_unary ...)
	
	// unit_unary()
	int *id;
	int tmp;
	int *addr;
	{
		if(!token) {
			printf("Error at line %lld: unexpected EOF\n", line);
			exit(-1);
		}
		if(token == Num) {
			match(Num);
		
			// emit code
			*++text = IMM;
			*++text = token_val;
			expr_type = INT;
		}
		else if(token == '"') {
			// continuous string
			// emit code
			
			*++text = IMM;
			*++text = token_val;

			match('"');
			// store the string
			while(token == '"') {
				match('"');
			}

			// append the end of string character '\0'
			// since all the data defaults to 0, we can just move one step forward
			data = (char *) (((int) data + sizeof(int)) & (-sizeof(int)));
			expr_type = PTR;
		}
		else if (token == Sizeof) {
			// sizeof is unary op
			// Supported : sizeof(int), sizeof(char) and sizeof(*...)

			match(Sizeof);
			match('(');
			expr_type = INT;

			if(token == Int){ 
				match(Int);
			} else if  (token == Char) {
				match(Char);
				expr_type = CHAR;
			}

			while (token == Mul) {
				match(Mul);
				expr_type = expr_type + PTR;
			}
			match(')');

			// emit code
			
			*++text = IMM;
			*++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);

			expr_type = INT;
		}
		else if (token == Id) {
			// 1. function call
			// 2. Enum variable
			// 3. global / local variable
			match(Id);

			id = current_id;

			if(token == '(') {
				// function call
				match('(');

				// pass in args
				tmp = 0; // Number of args
				while(token != ')') {
					expression(Assign);
					*++text = PUSH;
					tmp++;

					if(token == ',') {
						match(',');
					}
				}

				match(')');

				// emit code
				if(id[Class] == Sys) {
					// syscall
					*++text = id[Value];
				}
				else if (id[Class] == Fun) {
					// function call
					*++text = CALL;
					*++text = id[Value];
				}
				else {
					printf("Error at line %lld : Bad function call \n", line);
					exit(-1);
				}

				// Clean stack for args
				if(tmp > 0) {
					*++text = ADJ;
					*++text = tmp;
				}
				expr_type = id[Type];
			}
			else if (id[Class] == Num) {
				// enum
				*++text = IMM;
				*++text = id[Value];
				expr_type = INT;
			}
			else {
				// variable
				if(id[Class] == Loc) {
					*++text = LEA;
					*++text = index_of_bp - id[Value];
				}
				else if (id[Class] == Glo) {
					*++text = IMM;
					*++text = id[Value];
				}
				else {
					printf("Error at line %lld : Undefined variable\n", line);
					exit(-1);
				}

				// emit code, default behaviour is to load the value of the address
				// stored in ax
				expr_type = id[Type];
				*++text = (expr_type == Char) ? LC : LI;
			}
		}
		else if (token == '(') {
			// cast or parenthesis
			if (token == Int || token == Char) {
				tmp = (token == Char) ? CHAR : INT;
				match(token);

				while(token == Mul) {
					match(Mul);
					tmp = tmp + PTR;
				}

				match(')');

				expression(Inc); // cast has precedence as Inc

				expr_type = tmp;
			}
			else {
				// Normal parenthesis
				expression(Assign);
				match(')');
			}
		}
		else if (token == Mul) {
			// pointer dereference
			match(Mul);
			expression(Inc); // Dereference has precedence as Inc

			if (expr_type >= PTR) {
				expr_type = expr_type - PTR;
			} else {
				printf("Error at line %lld : Bad pointer dereference\n", line);
				exit(-1);
			}

			*++text = (expr_type == CHAR) ? LC : LI;
		}
		else if (token == And) {
			// address of
			match(And);
			expression(Inc); // Address of has precedence as Inc

			if(*text == LC || *text == LI) {
				text--;
			} else {
				printf("Error at line %lld : Bad address of\n", line);
				exit(-1);
			}

			expr_type += PTR;
		}
		else if (token == '!') {
			// Logical NOT
			
			match('!');
			expression(Inc);

			// emit code, use <expr> == 0
			*++text = PUSH;
			*++text = IMM;
			*++text = 0;
			*++text = EQ;

			expr_type = INT;
		}
		else if (token == '~') {
			// Bitwise NOT
			match('~');
			expression(Inc);

			// emit code, use <expr> XOR -1;
			*++text = PUSH;
			*++text = IMM;
			*++text = -1;
			*++text = XOR;

			expr_type = INT;
		}
		else if (token == Add) {
			// Unary plus, do nothing
			match(Add);
			expression(Inc);

			expr_type = INT;
		}
		else if (token == Sub) {
			// Unary minus
			match(Sub);

			if (token == Num) {
				*++text = IMM;
				*++text = -token_val;
				match(Num);
			} else {
				*++text = IMM;
				*++text = -1;
				*++text = PUSH;
				expression(Inc);
				*++text = MUL;
			}

			expr_type = INT;
		}
		else if (token == Inc || token == Dec) {
			tmp = token;
			match(token);
			expression(Inc);

			if(*text == LC) {
				*text = PUSH; // To duplicate address
				*++text = LC;
			} else if(*text == LI) {
				*text = PUSH;
				*++text = LI;
			} else {
				printf("Error at line %lld : Bad lvalue of pre-increment/decrement\n", line);
				exit(-1);
			}

			*++text = PUSH;
			*++text = IMM;
			*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
			*++text = (tmp == Inc) ? ADD : SUB;
			*++text = (expr_type == CHAR) ? SC : SI;
		}
		else {
			printf("Error at line %lld : Bad expression\n", line);
			exit(-1);
		}
	}

	// binary operator and postfix operators
	{
		while (token >= level) {
			// handle according to operator's current precedence

			tmp = expr_type;
			if(token == Assign) {
				// var = expr
				match(Assign);
				if(*text == LC || *text == LI) {
					*text = PUSH;
				} else {
					printf("Error at line %lld : Bad lvalue of assignment\n", line);
					exit(-1);
				}

				expression(Assign);

				expr_type = tmp;
				*++text = (expr_type == CHAR) ? SC : SI;
			} else if (token == Cond) {
				// expr ? a : b
				match(Cond);
				*++text = JZ;
				addr = ++text;

				expression(Assign);
				if(token == ':') {
					match(':');
				} else {
					printf("Error at line %lld : Missing ':'\n", line);
					exit(-1);
				}

				*addr = (int) (text + 3);
				*++text = JMP;
				addr = ++text;
				expression(Cond);
				*addr = (int) (text + 1);
			} else if (token == Lor) {
				// Logical OR
				match(Lor);

				*++text = JNZ;
				addr = ++text;
				expression(Lan);
				*addr = (int) (text + 1);
				expr_type = INT;
			} else if (token == Lan) {
				// Logical AND
				match(Lan);

				*++text = JZ;
				addr = ++text;
				expression(Or);
				*addr = (int) (text + 1);
				expr_type = INT;
			}
			else if (token == Or) {
				// Bitwise OR
				match(Or);

				*++text = PUSH;
				expression(Xor);
				*++text = OR;
				expr_type = INT;
			}
			else if (token == Xor) {
				// Bitwise XOR
				match(Xor);

				*++text = PUSH;
				expression(And);
				*++text = XOR;
				expr_type = INT;
			}
			else if (token == And) {
				// Bitwise AND
				match(And);

				*++text = PUSH;
				expression(Eq);
				*++text = AND;
				expr_type = INT;
			}
			else if (token == Eq) {
				// Equality
				match(Eq);

				*++text = PUSH;
				expression(Ne);
				*++text = EQ;
				expr_type = INT;
			}
			else if (token == Ne) {
				// Inequality
				match(Ne);

				*++text = PUSH;
				expression(Lt);
				*++text = NE;
				expr_type = INT;
			}
			else if (token == Lt) {
				// Less than
				match(Lt);

				*++text = PUSH;
				expression(Shl);
				*++text = LT;
				expr_type = INT;
			}
			else if (token == Gt) {
				// Greater than
				match(Gt);

				*++text = PUSH;
				expression(Shl);
				*++text = GT;
				expr_type = INT;
			}
			else if (token == Le) {
				// Less than or equal to
				match(Le);

				*++text = PUSH;
				expression(Shl);
				*++text = LE;
				expr_type = INT;
			}
			else if (token == Ge) {
				// Greater than or equal to
				match(Ge);

				*++text = PUSH;
				expression(Shl);
				*++text = GE;
				expr_type = INT;
			}
			else if (token == Shl) {
				// Shift left
				match(Shl);

				*++text = PUSH;
				expression(Add);
				*++text = SHL;
				expr_type = INT;
			}
			else if (token == Shr) {
				// Shift right
				match(Shr);

				*++text = PUSH;
				expression(Add);
				*++text = SHR;
				expr_type = INT;
			}
			else if (token == Add) {
				// Addition - check for pointer arithmetic
				match(Add);

				*++text = PUSH;
				expression(Mul);
				expr_type = tmp;

				if (expr_type > PTR) {
					// Pointer type and not a char *
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = MUL;
				}
				*++text = ADD;
			}
			else if (token == Sub) {
				// Subtraction - check for pointer arithmetic
				match(Sub);

				*++text = PUSH;
				expression(Mul);

				if (tmp > PTR && tmp == expr_type) {
					// Pointer Subtraction
					*++text = SUB;
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = DIV;
					expr_type = INT;
				} else if (tmp > PTR) {
					// Pointer Movement
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = MUL;
					*++text = SUB;
					expr_type = tmp;
				} else {
					// Numeral Subtraction
					*++text = SUB;
					expr_type = tmp;
				}
			}
			else if (token == Mul) {
				// Multiplication
				match(Mul);

				*++text = PUSH;
				expression(Inc);
				*++text = MUL;
				expr_type = tmp;
			}
			else if (token == Div) {
				// Division
				match(Div);

				*++text = PUSH;
				expression(Inc);
				*++text = DIV;
				expr_type = tmp;
			}
			else if (token == Mod) {
				// Modulo
				match(Mod);

				*++text = PUSH;
				expression(Inc);
				*++text = MOD;
				expr_type = tmp;
			}
			else if (token == Inc || token == Dec) {
				// postfix ++ and --
				// we will increase the value to the variable and
				// decrease it on ax to get its original value
				if(*text == LI) {
					*text = PUSH;
					*++text = LI;
				}
				else if (*text == LC) {
					*text = PUSH;
					*++text = LC;
				}
				else {
					printf("Error at line %lld: Bad value in increment/decrement\n", line);
					exit(-1);
				}

				*++text = PUSH;
				*++text = IMM;
				*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
				*++text == (token == Inc) ? ADD : SUB;
				*++text = (expr_type == CHAR) ? SC : SI;
				*++text = PUSH;
				*++text = IMM;
				*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
				*++text = (token == Inc) ? SUB : ADD;
				match(token);
			}
			else if (token == Brak) {
				// Array Index
				match(Brak);

				*++text = PUSH;
				expression(Assign);
				match(']');

				if(tmp > PTR) {
					// Pointer type
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = MUL;
				} else if (tmp < PTR) {
					printf("Error at line %lld : Pointer type expected \n", line);
					exit(-1);
				}

				expr_type = tmp - PTR;
				*++text = ADD;
				*++text = (expr_type == CHAR) ? LC : LI;
			}
			else {
				printf("Compiler Error at line %lld: Expected operator, found %lld\n", line, token);
				exit(-1);
			}
		}
	}
}


void statement() {
	// There are 8 kinds of statements here:
	// 1. if (...) <statement> [else <statement>]
	// 2. while (...) <statement>
	// 3. { <statement> }
	// 4. return ...;
	// 5. <empty statement>;
	// 6. <expression

	int *a, *b;

	if (token == If) {
		// if (...) <statement> [else <statement>]
		//
		// if (...)            <cond>
		//                     JZ a
		//   <statement>       <statement>
		// else                JMP b
		//
		// a:
		//  <statement>       <statement>
		// b:                 b:

		match(If);
		match('(');
		expression(Assign);    // Parse condition
		match(')');

		// Emit code for if
		*++text = JZ;
		b = ++text;

		statement();           // Parse statement
		if(token == Else) {
			match(Else);

			// Emit code for JMP B;
			*b = (int) (text + 3);
			*++text = JMP;
			b = ++text;

			statement();       // Parse else statement
		}

		*b = (int) (text + 1);
	}
	else if (token == While) {
		//
		// a:                   a:
		//   while (<cond>)        <cond>
		//                         JZ b
		//   <statement>           <statement>
		//                         JMP a
		// b:                   b:
		match(While);

		a = text + 1;

		match('(');
		expression(Assign);    // Parse condition
		match(')');

		// Emit code for while
		*++text = JZ;
		b = ++text;

		statement();           // Parse statement

		*++text = JMP;
		*text  = (int) a;
		*b = (int) (text + 1);
	}
	else if (token == '{') {
		// { <statement> }
		match('{');

		while (token != '}') {
			statement();
		}

		match('}');
	}
	else if (token == Return) {
		// return [expression];
		match(Return);

		if(token != ';') {
			expression(Assign);
		}

		match(';');

		// Emit code for return
		*++text = LEV;
	}
	else if (token == ';') {
		// <empty statement>;
		match(';');
	}
	else {
		// <expression>;
		expression(Assign);
		match(';');
	}
}

void function_parameter() {
	int type;
	int params;

	params = 0;

	while (token != ')') {
		// int name, ...

		type = INT;
		if(token == Int) {
			match(Int);
		} else if (token == Char) {
			type = CHAR;
			match(Char);
		}

		// Pointer Type
		while (token == Mul) {
			match(Mul);
			type += PTR;
		}

		// Parameter Name
		if(token != Id) {
			printf("Error at line %lld: Bad parameter declaration\n", line);
			exit(-1);
		}
		if(current_id[Class] == Loc) {
			printf("Error at line %lld: Duplicate parameter declaration\n", line);
			exit(-1);
		}

		match(Id);

		// Store the local variable
		current_id[BClass] = current_id[Class];
		current_id[Class] = Loc;

		current_id[BType] = current_id[Type];
		current_id[Type] = type;

		current_id[BValue] = current_id[Value];
		current_id[Value] = params++;

		if(token == ',') {
			match(',');
		}
	}

	index_of_bp = params + 1;
}

void function_body() {
	// ... {
	// 1. Local declarations
	// 2. Statements
	// }
	
	int pos_local;
	int type;
	pos_local = index_of_bp;

	while(token == Int || token == Char) {
		// Local variable declaration
		basetype = (token == Int) ? INT : CHAR;
		match(token);

		while(token != ';') {
			type = basetype;

			while(token == Mul) {
				match(Mul);
				type += PTR;
			}

			if(token != Id) {
				printf("Error at line %lld: Bad local declaration\n", line);
				exit(-1);
			}
			if(current_id[Class] == Loc) {
				printf("Error at line %lld: Duplicate local declaration\n", line);
				exit(-1);
			}
			match(Id);

			// Store the local variable
			
			current_id[BClass] = current_id[Class];
			current_id[Class] = Loc;

			current_id[BType] = current_id[Type];
			current_id[Type] = type;

			current_id[BValue] = current_id[Value];
			current_id[Value] = ++pos_local;

			if(token == ',') {
				match(',');
			}
		}
		match(';');
	}

	// Save the stack size for local variables
	*++text = ENT;
	*++text = pos_local - index_of_bp;

	// Statements
	while (token != '}') {
		statement();
	}

	// Emit code for leaving subfunction
	*++text = LEV;
}

void function_declaration() {
	// type func_name (...) {...}
	
	match('(');
	function_parameter();
	match(')');

	match('{');
	function_body();
	// match('}');
	
	current_id = symbols;
	while(current_id[Token]) {
		if (current_id[Class] == Loc) {
			current_id[Class] = current_id[BClass];
			current_id[Type]  = current_id[BType];
			current_id[Value] = current_id[BValue];
		}
		current_id += IdSize;
	}
}

void global_declaration() {
	// global_declaration ::= enum_decl | variable_decl | function_decl
	//
	// enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num']} '}'
	//
	// variable_decl ::= type {'*'} id {',' {'*'} id } ';'
	//
	// function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'
	
	int type; // tmp, actual type for variable

	basetype = INT;

	// parse enum
	if (token == Enum) {
		// enum [id] { a = 10, b = 20, ... }
		
		match(Enum);

		if(token != '{') {
			match(Id); // skip the [id] part
		}
		if(token == '{') {
			match('{');
			enum_declaration();
			match('}');
		}

		match(';');
		return;
	}

	// parse type information
	if(token == Int) {
		match(Int);
	}
	else if (token == Char) {
		match(Char);
		basetype = CHAR;
	}

	// parse comma separated variable declaration
	while (token != ';' && token != '}') {
		type = basetype;

		// parse pointer
		while (token == Mul) {
			match(Mul);
			type = type + PTR;
		}

		if (token != Id) {
			// invalid declaration
			printf("Error at line %lld: Expected identifier\n", line);
			exit(-1);
		}
		if (current_id[Class]) {
			// identifier exists
			printf("Error at line %lld : Duplicate global declaration\n", line);
			exit(-1);
		}
		match(Id);
		current_id[Type] = type;

		if (token == '(') {
			current_id[Class] = Fun;
			current_id[Value] = (int) (text + 1);
			function_declaration();
		} else {
			// variable declaration
			current_id[Class] = Glo;   // Global variable
			current_id[Value] = (int) data;
			data = data + sizeof(int);
		}

		if (token == ',') {
			match(',');
		}
	}
	next();
}

void program() {
	next();

	while (token > 0) {
		global_declaration();
	}
}

int eval() {
	int op, *tmp;

	while(1) {
		op = *pc++;

		// Memory Ops

		if (op == IMM) {          // Put an Immediate value into register AX
			if(debug) printf("\tIMM : ax <- (%lld | %p)\n", *pc, (void *) *pc);
			ax = *pc++;
		}
		else if (op == LC) {    // Load a character into AX from a memory address stored at AX
			if(debug) printf("\tLC\n");
			ax = *(char *) ax;
		}
		else if (op == LI) {    // Load an int into AX from a memory address stored at AX
			if(debug) printf("\tLI : ax <- *ax(%lld)\n", *(int *) ax);
			ax = *(int *) ax;
		}
		else if (op == SC) {    // Store the character held at AX to a memory address at the top of the stack
			if(debug) printf("\tSC\n");
			*(char *)*sp++ = ax;
		}
		else if (op == SI) {
			if(debug) printf("\tSI : *sp[%p] <- ax(%lld)\n", sp, ax);
			*(int *)*sp++ = ax;
		}

		// Stack Ops

		else if (op == PUSH) {  // Push the value of AX onto the stack
			if(debug) printf("\tPUSH: (%lld | %p) -> Stack @ %p\n", ax, (void *) ax, sp - 1);
			*--sp = ax;
		}

		// Jump Ops

		else if (op == JMP) {   // Jump
			if(debug) printf("\tJMP\n");
			pc = (int *) *pc;
		}
		else if (op == JZ) {    // Jump if ax is zero
			if(debug) printf("\tJZ : %lld ? [%p] : [%p]\n", ax, pc + 1, (int *) *pc);
			pc = ax ? pc + 1 : (int *)*pc;
		}
		else if (op == JNZ) {   // Jump if ax is not zero
			if(debug) printf("\tJNZ\n");
			pc = ax ? (int *)*pc : pc + 1;
		}

		// Function call related Ops

		else if (op == CALL) {  // Call a subroutine
			if(debug) printf("\tCALL: Pushing pc+1[%p] to Stack @ [%p]\n", pc + 1, sp - 1);
			*--sp = (int)(pc + 1);
			if(debug) printf("\t    : Jumping to %p\n", (int *) *pc);
			pc = (int *) *pc;
		}
		// else if (op == RET) {
			// pc = (int *) *sp++;
		//}
		else if (op == ENT) { // Make a new stack frame
			// Will store the current PC value onto the stack, and
			// save some space <size> bytes for local variables
			if(debug) printf("\tENT : *--sp[%p] = bp[%p]\n", sp - 1, bp);
			*--sp = (int) bp;
			if(debug) printf("\t    : sp <- bp[%p]\n", bp);
			bp = sp;
			if(debug) printf("\t    : sp[%p] -= *pc[%p](%lld) = [%p]\n", sp, pc, *pc, sp - *pc);
			sp = sp - *pc++;
		}
		else if (op == ADJ) { // Adjust the stack pointer
			if(debug) printf("\tADJ : sp += pc(%lld) = [%p]\n", *pc, sp + *pc);
			// Remove arguments from frame
			sp += *pc++;
		}
		else if (op == LEV) { // Restore call frame and PC
			if(debug) printf("\tLEV : Setting sp <- bp[%p]\n", bp);
			sp = bp;
			if(debug) printf("\t    : Setting bp <- *sp(%lld | 0x%llx)++\n", *sp, *sp);
			bp = (int *) *sp++; // Base pointer stored on stack
			if(debug) printf("\t    : Setting pc <- *sp(%lld | 0x%llx)++\n", *sp, *sp);
			pc = (int *) *sp++; // Program counter stored on stack
			if(debug) printf("\t    * New value at PC[%p] = (%lld | 0x%llx)\n", pc, *pc, *pc);
		}
		else if (op == LEA) { // Load effective address
			if(debug) printf("\tLEA : ax <- bp(%lld) + *pc(%lld)[%p] = (%lld | 0x%llx)\n", (long long) bp, *pc, pc, (int) (bp + *pc), (int) (bp + *pc));
			ax = (int) (bp + *pc++);
		}

		// Math Ops

		else if (op == OR) {
			if(debug) printf("\tOR\n");
			ax = *sp++ | ax;
		}
		else if (op == XOR) {
			if(debug) printf("\tXOR\n");
			ax = *sp++ * ax;
		}
		else if (op == AND) {
			if(debug) printf("\tAND\n");
			ax = *sp++ & ax;
		}
		else if (op == EQ) {
			if(debug) printf("\tEQ\n");
			ax = *sp++ == ax;
		}
		else if (op == NE) {
			if(debug) printf("\tNE\n");
			ax = *sp++ != ax;
		}
		else if (op == LT) {
			if(debug) printf("\tLT\n");
			ax = *sp++ < ax;
		}
		else if (op == GT) {
			if(debug) printf("\tGT : %lld > %lld\n", *sp, ax);
			ax = *sp++ > ax;
		}
		else if (op == LE) {
			if(debug) printf("\tLE\n");
			ax = *sp++ <= ax;
		}
		else if (op == GE) {
			if(debug) printf("\tGE\n");
			ax = *sp++ >= ax;
		}
		else if (op == SHL) {
			if(debug) printf("\tSHL\n");
			ax = *sp++ << ax;
		}
		else if (op == SHR) {
			if(debug) printf("\tSHR\n");
			ax = *sp++ >> ax;
		}
		else if (op == ADD) {
			if(debug) printf("\tADD : %lld + %lld = %lld\n", *sp, ax, *sp + ax);
			ax = *sp++ + ax;
		}
		else if (op == SUB) {
			if(debug) printf("\tSUB\n");
			ax = *sp++ - ax;
		}
		else if (op == MUL) {
			if(debug) printf("\tMUL : %lld * %lld = %lld\n", *sp, ax, *sp * ax);
			ax = *sp++ * ax;
		}
		else if (op == DIV) {
			if(debug) printf("\tDIV\n");
			ax = *sp++ / ax;
		}
		else if (op == MOD) {
			if(debug) printf("\tMOD\n");
			ax = *sp++ % ax;
		}

		// Built in Instructions
		
		else if (op == EXIT) {  // Exit the program
			if(debug) printf("exit(%lld)\n", *sp);
			return *sp;
		}
		else if (op == OPEN) { // Open File
			if(debug) printf("\tOPEN\n");
			ax = open((char *) sp[1], sp[0]);
		}
		else if (op == CLOS) { // Close file
			if(debug) printf("\tCLOS\n");
			ax = close(sp[0]);
		}
		else if (op == READ) { // Read from file
			if(debug) printf("\tREAD\n");
			ax = read(sp[2], (char *) sp[1], sp[0]);
		}
		else if (op == PRTF) { // Printf
			if(debug) printf("\tPRTF\n");
			tmp = sp + pc[1];
			ax = printf((char *) tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);
		}
		else if (op == MALC) { // Malloc
			if(debug) printf("\tMALC\n");
			ax = (int) malloc(*sp);
		}
		else if (op == MSET) { // Memset
			if(debug) printf("\tMSET\n");
			ax = (int) memset((char *) sp[2], sp[1], sp[0]);
		}
		else if (op == MCMP) { // Memcmp
			if(debug) printf("\tMCMP\n");
			ax = memcmp((char *) sp[2], (char *) sp[1], sp[0]);
		}

		else {
			printf("Unknown opcode: %lld | 0x%llx\n", op, op);
			return -1;
		}
	}

	return 0;
}

int32_t main(int32_t argc, char **argv) {
	int i, fd;
	int *tmp;

	argc--;
	argv++;

	poolsize = 1024 * 1024; // arbirary size
	line = 1;

	if((fd = open(*argv, 0)) < 0) {
		printf("could not open(%s)\n", *argv);
		return 1;
	}

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
	if(!(symbols = malloc(poolsize))) {
		printf("could not malloc(%lld) for symbols area\n", poolsize);
		return 1;
	}

	memset(text, 0, poolsize);
	memset(data, 0, poolsize);
	memset(stack, 0, poolsize);
	memset(symbols, 0, poolsize);

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

	if(!(src = old_src = malloc(poolsize))) {
		printf("could not malloc(%lld) for source area\n", poolsize);
		return 1;
	}

	// read the source file
	if((i = read(fd, src, poolsize - 1)) <= 0) {
		printf("read() returned %lld\n", i); // TODO read returned -1
		return 1;
	}

	src[i] = 0;
	close(fd);

	program();

	if (!(pc = (int *)idmain[Value])) {
        printf("main() not defined\n");
        return -1;
    }

	// setup stack
    sp = (int *)((int)stack + poolsize);
    *--sp = EXIT; // call exit if main returns
    *--sp = PUSH; tmp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmp;

	return eval();
}
