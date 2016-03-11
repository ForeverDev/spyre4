#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "spyre.h"

spy_state* 
spy_newstate() {

	spy_state* S = malloc(sizeof(spy_state));	
	S->ip = NULL;
	S->sp = (uint64 *)&S->memory[START_STACK + SIZE_STACK];
	S->bp = (uint64 *)&S->memory[START_STACK + SIZE_STACK];
	S->code = NULL; /* to be allocated in spy_run */
	
	/* setup the memory map */
	S->heap_data = malloc(sizeof(spy_memoryMap));
	S->heap_data->head = NULL;
	S->heap_data->tail = NULL;
	S->heap_data->size = 0;

	return S;

}

static inline
void
spy_pushFromCode(spy_state* S, size_t bytes) {
	for (uint32 i = 0; i < bytes; i++) {
		*((uint8 *)S->sp - i - 1) = *S->ip++; 
	}
	S->sp--;
}

static inline
void
spy_pushInt(spy_state* S, uint64 value) {
	*--S->sp = value;
}

static inline
void
spy_pushFloat(spy_state* S, float64 value) {
	*(float64 *)--S->sp = value;
}

static inline
uint64
spy_popInt(spy_state* S) {
	return *S->sp++;
}

static inline
float64
spy_popFloat(spy_state* S) {
	return *(float64 *)S->sp++;
}

static inline
uint8
spy_readInt8(spy_state* S) {
	return *S->ip++;
}

static inline
uint16
spy_readInt16(spy_state* S) {
	uint16 result = *(uint16 *)S->ip;
	S->ip += 2;
	return result;
}

static inline
uint32
spy_readInt32(spy_state* S) {
	uint32 result = *(uint32 *)S->ip;
	S->ip += 4;
	return result;
}

static inline
uint64
spy_readInt64(spy_state* S) {
	uint64 result = *(uint64 *)S->ip;
	S->ip += 8;
	return result;
}

static
void
spy_dumpStack(spy_state* S) {
	printf("STACK DUMP:\n");
	printf("SP: %08llx\n", (uint64)((uint8 *)S->sp - S->memory));
	for (uint8* i = &S->memory[START_STACK + SIZE_STACK - 1]; i >= (uint8 *)S->sp; i--) {
		printf("0x%08lx: %02x\n", i - S->memory, *i);	
	}
}

void 
spy_run(spy_state* S, const uint8* code, size_t size) {
	
	/* use an extension in GCC that allows for direct threading... this way we
	 * don't have to test every opcode in a switch/case... this makes for
	 * significantly faster interpretation
	 */
	static const void* dispatch[] = {
		&&null,		&&noop,		&&quit,
		&&push_b,	&&push_i,	&&push_f,	&&add_i,
		&&sub_i,	&&mul_i,	&&div_i,	&&pow_i,	
		&&mod_i,	&&shl_i,	&&shr_i,	&&and_i,
		&&or_i,		&&xor_i,	&&not_i,	&&ftoi,
		&&itof,		&&call,		&&ret_i,	&&ret_f,
		&&put_c,	&&put_i,	&&put_f
	};

	/* allocate space for S->code to store the code */
	if (S->code) free(S->code);
	S->code = malloc(size + 1);
	S->ip = S->code;
	
	/* copy the argument code into S->code */
	memcpy(S->code, code, size);
	S->code[size] = 0;

	uint8 opcode;

	/* some variables that may be used by each instruction */
	uint8	a8, b8;
	uint16	a16, b16;
	uint32	a32, b32;
	uint64	a64, b64;
	float64	fa64, fb64;


	while ((opcode = *S->ip++)) {
		
		/* direct threading is significantly faster than
		 * using a switch in this case because we don't have
		 * to test every opcode... just jump right to the
		 * correct label
		 */
		goto *dispatch[opcode];	
		continue;
null:
		break;
noop:
		continue;
quit:
		break;	
push_b:
		spy_pushFromCode(S, SIZEOF_BYTE);
		continue;
push_i:
		spy_pushFromCode(S, SIZEOF_INT);
		continue;
push_f:
		spy_pushFromCode(S, SIZEOF_FLOAT);
		continue;
add_i:
		b64 = spy_popInt(S);
		a64 = spy_popInt(S);
		spy_pushInt(S, a64 + b64);
		continue;
sub_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, spy_popInt(S) - b64);
		continue;
mul_i:
		spy_pushInt(S, spy_popInt(S) * spy_popInt(S));
		continue;
div_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, spy_popInt(S) / b64);
		continue;
pow_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, pow(spy_popInt(S), b64));
		continue;	
mod_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, spy_popInt(S) % b64);
		continue;
shl_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, spy_popInt(S) << b64);
		continue;
shr_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, spy_popInt(S) >> b64);
		continue;
and_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, spy_popInt(S) & b64);
		continue;
or_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, spy_popInt(S) | b64);
		continue;
xor_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, spy_popInt(S) ^ b64);
		continue;
not_i:
		b64 = spy_popInt(S);
		spy_pushInt(S, ~spy_popInt(S));
		continue;
ftoi:
		/* safe cast from 64 bit float to 64 bit int */
		spy_pushInt(S, (uint64)spy_popFloat(S));
		continue;
itof:	
		/* unsafe cast from 64 bit in to 64 bit float */
		spy_pushFloat(S, (float64)spy_popInt(S));
		continue;
call:	
		/* read target location from code */
		a32 = spy_readInt32(S);
		/* read number of arguments from code */
		a16 = spy_readInt16(S);
		
		/* push nargs */
		spy_pushInt(S, (uint64)a16);
		/* push bp */
		spy_pushInt(S, (uint64)S->bp);
		/* push return address */
		spy_pushInt(S, (uint64)S->ip);

		/* setup function environment and jump */
		S->bp = S->sp;
		S->ip = &S->code[a32];

		continue;
ret_i:	
		a64 = spy_popInt(S); /* return value */	
		S->sp = S->bp;	
		S->ip = (uint8 *)spy_popInt(S);
		S->bp = (uint64 *)spy_popInt(S);
		S->sp += spy_popInt(S);		
		spy_pushInt(S, a64);
		continue;
ret_f:
		fa64 = spy_popFloat(S); /* return value */	
		S->sp = S->bp;	
		S->ip = (uint8 *)spy_popInt(S);
		S->bp = (uint64 *)spy_popInt(S);
		S->sp += spy_popInt(S);		
		spy_pushFloat(S, fa64);
		continue;
put_c:
		printf("%c\n", (sint8)spy_popInt(S));
		continue;
put_i:
		printf("%llu\n", spy_popInt(S));
		continue;
put_f:		
		printf("%lf\n", spy_popFloat(S));
		continue;

		continue;
	}	

	spy_dumpStack(S);

}
