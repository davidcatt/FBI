/*  FBI 1.2 - A Fast Brainfuck Interpreter
    Copyright (C) 2012  David Catt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define LOWMEM //Uses 4x less memory but runs slower
#define INFINITE //The tape is infinite but runs slower
#define TRACKTAPE //The tape is expanded dynamically if infinite
#define COLBASE 65536 //The starting size of a collection or stack
#define MEMORDER 18 //The size of the tape in 2^n format
typedef struct IntStack {
	int* stack;
	size_t stPtr;
	size_t stSz;
} IntStack, *PIntStack;
PIntStack IntStack_new() {
	PIntStack ni = (PIntStack) malloc(sizeof(IntStack));
	if(ni == NULL) { return NULL; };
	ni->stSz = COLBASE;
	ni->stPtr = 0;
	ni->stack = (int*) malloc(sizeof(int) * ni->stSz);
	if(ni->stack == NULL) {
		free(ni);
		return NULL;
	};
	return ni;
};
void IntStack_push(PIntStack s, int val) {
	int* tptr;
	s->stack[s->stPtr] = val;
	s->stPtr++;
	if(s->stPtr >= s->stSz) {
		s->stSz <<= 1;
		tptr = (int*) realloc(s->stack, sizeof(int) * s->stSz);
		if(tptr == NULL) { free(s->stack); };
		s->stack = tptr;
	};
};
int IntStack_pop(PIntStack s) {
	if(s->stPtr <= 0) { return 0; };
	s->stPtr--;
	return s->stack[s->stPtr];
};
void IntStack_free(PIntStack s) {
	if(s != NULL) {
		if(s->stack != NULL) { free(s->stack); };
		free(s);
		s = NULL;
	};
};
typedef enum OpCode {
	NOP = -1,
	ADJ = 0,
	VAL = 1,
	MOV = 2,
	INP = 3,
	OUT = 4,
	BLK = 5,
	FIN = 6,
} OpCode;
typedef struct BFOp {
	OpCode ins;
	int val;
} BFOp, *PBFOp;
BFOp BFOp_create(OpCode i, int v) {
	BFOp nb;
	nb.ins = i;
	nb.val = v;
	return nb;
};
typedef struct CodeCol {
	BFOp* ops;
	int size;
	int memSz;
} CodeCol, *PCodeCol;
PCodeCol CodeCol_new() {
	PCodeCol nc = (PCodeCol) malloc(sizeof(CodeCol));
	if(nc == NULL) { return NULL; };
	nc->memSz = COLBASE;
	nc->ops = (BFOp*) malloc(sizeof(BFOp) * nc->memSz);
	nc->size = 0;
	if(nc->ops == NULL) {
		free(nc);
		return NULL;
	};
	return nc;
};
void CodeCol_trim(PCodeCol c) {
	BFOp* tptr;
	if(c->size > 0) {
		c->memSz = c->size;
		tptr = (BFOp*) realloc(c->ops, sizeof(BFOp) * c->memSz);
		if(tptr == NULL) { free(c->ops); };
		c->ops = tptr;
	};
};
void CodeCol_add(PCodeCol c, BFOp op) {
	BFOp* tptr;
	c->ops[c->size] = op;
	c->size++;
	if(c->size >= c->memSz) {
		c->memSz <<= 1;
		tptr = (BFOp*) realloc(c->ops, sizeof(BFOp) * c->memSz);
		if(tptr == NULL) { free(c->ops); };
		c->ops = tptr;
	};
};
void CodeCol_add_raw(PCodeCol c, OpCode o, int v) {
	BFOp* tptr;
	c->ops[c->size].ins = o;
	c->ops[c->size].val = v;
	c->size++;
	if(c->size >= c->memSz) {
		c->memSz <<= 1;
		tptr = (BFOp*) realloc(c->ops, sizeof(BFOp) * c->memSz);
		if(tptr == NULL) { free(c->ops); };
		c->ops = tptr;
	};
};
void CodeCol_free(PCodeCol c) {
	if(c != NULL) {
		if(c->ops != NULL) { free(c->ops); };
		free(c);
		c = NULL;
	};
};
#ifdef LOWMEM
typedef char CELL;
#else
typedef int CELL;
#endif
#ifndef INFINITE
const int TapeSize = 1 << MEMORDER;
const int TapeMask = (1 << MEMORDER) - 1;
#endif
PCodeCol loadSrc(const char* src) {
	//Create variables
	PCodeCol cc;
	PIntStack ls;
	FILE* inp;
	int c = 0;
	int last = -1;
	int cur = 0;
	int count = 0;
	int tmp = 0;
	//Init variables
	cc = CodeCol_new();
	ls = IntStack_new();
	//Open the file
	inp = fopen(src, "r");
	//Do checks
	if((inp == NULL) || (cc == NULL) || (ls == NULL)) {
		//Free any allocated memory
		if(inp != NULL) { fclose(inp); };
		if(cc != NULL) { CodeCol_free(cc); };
		if(ls != NULL) { IntStack_free(ls); };
		//Return a null pointer to indicate failiure
		return NULL; 
	};
	//Read code
	while(c != -1) {
		//Get input
		c = fgetc(inp);
		//Find instruction type
		switch(c) {
			case '+':
				cur = 0;
				break;
			case '-':
				cur = 0;
				break;
			case '<':
				cur = 1;
				break;
			case '>':
				cur = 1;
				break;
			case ',':
				cur = 2;
				break;
			case '.':
				cur = 3;
				break;
			case '[':
				cur = 4;
				break;
			case ']':
				cur = 5;
				break;
			default:
				cur = -1;
				break;
		};
		//Only parse instruction if it is recognizable or an EOF
		if((cur != -1) || (c == -1)) {
			//Flush runs to the code collection
			if(cur != last) {
				switch(last) {
					case 0:
						count &= 255;
						if(count != 0) { CodeCol_add_raw(cc, ADJ, count); };
						count = 0;
						break;
					case 1:
						if(count != 0) { CodeCol_add_raw(cc, MOV, count); };
						count = 0;
						break;
					case 2:
						if(count > 0) { CodeCol_add_raw(cc, INP, count); };
						count = 0;
						break;
					case 3:
						if(count > 0) { CodeCol_add_raw(cc, OUT, count); };
						count = 0;
						break;
				};
				//Check memory
				if(cc->ops == NULL) {
					CodeCol_free(cc);
					IntStack_free(ls);
					fclose(inp);
					return NULL;
				};
			};
			//Parse the current instruction
			switch(cur) {
				case 0:
					if(c == '+') {
						count++;
					} else if(c == '-') {
						count--;
					};
					break;
				case 1:
					if(c == '>') {
						count++;
					} else if(c == '<') {
						count--;
					};
					break;
				case 2:
					count++;
					break;
				case 3:
					count++;
					break;
				case 4:
					count = 0;
					CodeCol_add_raw(cc, BLK, 0);
					break;
				case 5:
					count = 0;
					CodeCol_add_raw(cc, FIN, 0);
					break;
			};
			//Check memory
			if(cc->ops == NULL) {
					CodeCol_free(cc);
					IntStack_free(ls);
					fclose(inp);
					return NULL;
			};
			//Update the last instruction
			last = cur;
		};
	};
	//Close the source file
	fclose(inp);
	//Convert [+] and [-] constructs into explicit zeros and merge ADJ and VAL operations
	cur = 0;
	count = 0;
	while(count < cc->size) {
		//Check for a zeroing construct
		if((count + 2) < cc->size) {
			if((cc->ops[count].ins == BLK) && (cc->ops[count + 1].ins == ADJ) && (cc->ops[count + 2].ins == FIN)) {
				//Overwrite previous null changes
				if((count > 0) && (cur > 0)) {
					if((cc->ops[count].ins == ADJ) || (cc->ops[count].ins == VAL)) { cur--; };
				};
				//Convert to an explicit VAL and merge any sequential ADJ operations
				if((count + 3) < cc->size) {
					if(cc->ops[count + 3].ins == ADJ) {
						//Create merged VAL
						cc->ops[cur].ins = VAL;
						cc->ops[cur].val = cc->ops[count + 3].val;
						count += 4;
					} else {
						//Create VAL instruction
						cc->ops[cur].ins = VAL;
						cc->ops[cur].val = 0;
						count += 3;
					};
				} else {
					//Create VAL instruction
					cc->ops[cur].ins = VAL;
					cc->ops[cur].val = 0;
					count += 3;
				};
			} else if((cc->ops[cur].ins == ADJ) && (cc->ops[cur + 1].ins == ADJ)) {
				//Merge matching ADJ operands
				cc->ops[cur].val = (cc->ops[cur].val + cc->ops[cur + 1].val) & 255;
				count += 2;
			} else {
				//Copy instruction
				cc->ops[cur].ins = cc->ops[count].ins;
				cc->ops[cur].val = cc->ops[count].val;
				count++;
			};
		} else if((count + 1) < cc->size) {
			//Check for matching ADJ operands
			if((cc->ops[cur].ins == ADJ) && (cc->ops[cur + 1].ins == ADJ)) {
				cc->ops[cur].val = (cc->ops[cur].val + cc->ops[cur + 1].val) & 255;
				count += 2;
			} else {
				//Copy instruction
				cc->ops[cur].ins = cc->ops[count].ins;
				cc->ops[cur].val = cc->ops[count].val;
				count++;
			};
		} else {
			//Copy instruction
			cc->ops[cur].ins = cc->ops[count].ins;
			cc->ops[cur].val = cc->ops[count].val;
			count++;
		};
		//Update loops
		switch(cc->ops[cur].ins) {
			case BLK:
				IntStack_push(ls, cur);
				break;
			case FIN:
				//If stack is empty, return null pointer
				if(ls->stPtr == 0) {
					CodeCol_free(cc);
					IntStack_free(ls);
					return NULL;
				};
				//Update the loop
				tmp = IntStack_pop(ls);
				cc->ops[tmp].val = cur;
				cc->ops[cur].val = tmp;
				break;
		};
		//Check memory
		if(ls->stack == NULL) {
			IntStack_free(ls);
			CodeCol_free(cc);
			return NULL;
		};
		//Increment pointer
		cur++;
	};
	//Update code size
	cc->size = cur;
	//Check braces
	if(ls->stPtr > 0) {
		IntStack_free(ls);
		CodeCol_free(cc);
		return NULL;
	};
	//Cleanup
	IntStack_free(ls);
	CodeCol_trim(cc);
	if(cc->ops == NULL) {
		CodeCol_free(cc);
		return NULL;
	} else {
		return cc;
	};
};
int runCode(PCodeCol code) {
	//Setup variables
#ifdef INFINITE
	int TapeSize = 1 << MEMORDER;
	CELL* ttp = NULL;
	int mod = 0;
	int old = 0;
#ifdef TRACKTAPE
	int lexp = 1 << MEMORDER;
	int rexp = 1 << MEMORDER;
#endif
#endif
	CELL* tape = (CELL*) malloc(sizeof(CELL) * TapeSize);
	int ptr = TapeSize >> 1;
	int idx = 0;
	int tmp = 0;
	//Check the tape
	if(tape == NULL) { return 1; };
	//Zero the tape
	memset(tape, 0, sizeof(CELL) * TapeSize);
	//Execute the code
	while(idx < code->size) {
		switch(code->ops[idx].ins) {
			case ADJ:
				tape[ptr] = (CELL) ((tape[ptr] + code->ops[idx].val) & 255);
				break;
			case VAL:
				tape[ptr] = (CELL) code->ops[idx].val;
				break;
			case MOV:
#ifndef INFINITE
				ptr = (ptr + code->ops[idx].val) & TapeMask;
#else
				ptr += code->ops[idx].val;
				if(ptr < 0) {
					mod = 0;
					old = TapeSize;
					while(ptr < 0) {
#ifndef TRACKTAPE
						ptr += (TapeSize >> 1);
						mod += (TapeSize >> 1);
						TapeSize <<= 1;
#else
						ptr += lexp;
						mod += lexp;
						TapeSize += lexp;
						lexp <<= 1;
#endif
					};
					ttp = (CELL*) tape;
					tape = (CELL*) malloc(sizeof(CELL) * TapeSize);
					if(tape == NULL) {
						free(ttp);
						return 1;
					};
					memset(tape, 0, sizeof(CELL) * TapeSize);
					memcpy((CELL*) tape + mod, ttp, sizeof(CELL) * old);
					free(ttp);
					ttp = NULL;
				} else if(ptr >= TapeSize) {
					mod = 0;
					old = TapeSize;
					while(ptr >= TapeSize) {
#ifndef TRACKTAPE
						ptr += (TapeSize >> 1);
						mod += (TapeSize >> 1);
						TapeSize <<= 1;
#else
						TapeSize += rexp;
						rexp <<= 1;
#endif
					};
					ttp = (CELL*) tape;
					tape = (CELL*) malloc(sizeof(CELL) * TapeSize);
					if(tape == NULL) {
						free(ttp);
						return 1;
					};
					memset(tape, 0, sizeof(CELL) * TapeSize);
					memcpy((CELL*) tape + mod, ttp, sizeof(CELL) * old);
					free(ttp);
					ttp = NULL;
				};
#endif
				break;
			case INP:
				tmp = code->ops[idx].val;
				while(tmp > 0) {
					tape[ptr] = (CELL) (getchar() & 255);
					tmp--;
				};
				break;
			case OUT:
				tmp = code->ops[idx].val;
				while(tmp > 0) {
					putchar(tape[ptr]);
					tmp--;
				};
				break;
			case BLK:
				if(!tape[ptr]) { idx = code->ops[idx].val; };
				break;
			case FIN:
				if(tape[ptr]) { idx = code->ops[idx].val; };
				break;
		};
		idx++;
	};
	free(tape);
	return 0;
};
int main(int argc, const char* argv[]) {
	PCodeCol prog;
	int idx = 0;
	int err = 0;
	if(argc < 2) {
		printf("usage: FBI [src] <src> <src> ...\n");
		return 1;
	} else {
		for(idx = 1; idx < argc; idx++) {
			prog = loadSrc(argv[idx]);
			if(prog != NULL) {
				err += runCode(prog);			
			} else {
				printf("%s: Error while loading\n", argv[idx]);
				err++;
			};
		};
		return err;
	};
};