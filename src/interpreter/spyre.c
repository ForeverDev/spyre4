#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "spyre.h"

/* function:	spy_newstate
 *
 * desc:		creates and initializes a Spyre Virtual Machine
 *				state capable of running Spyre bytecode.  The
 *				VM state is initialized.  The only uninitialized
 *				field is member code, which will be initialized
 *				in method spy_run
 *
 * returns:		a Spyre virtual machine state
 */
spy_state* 
spy_newstate() {

	spy_state* S = malloc(sizeof(spy_state));	
	S->ip = 0;
	S->sp = START_STACK;
	S->bp = START_STACK;
	S->code = NULL; /* to be allocated in spy_run */
	
	/* setup the memory map */
	S->heap_data = malloc(sizeof(spy_memoryMap));
	S->heap_data->head = NULL;
	S->heap_data->tail = NULL;
	S->heap_data->size = 0;

	return S;

}

/* function:	spy_pushFromCode
 *
 * desc:		copies @bytes number of bytes from code memory
 *				to the stack.  it leaves the stack aligned to
 *				an 8 byte boundary.  it expects that IP is
 *				pointing to the first byte to copy... it also
 *				handles moving IP forward @bytes number of bytes.
 *
 * returns:		nothing
 */
static inline
void
spy_pushFromCode(spy_state* S, size_t bytes) {
	memset(&S->memory[S->sp + 1], 0, STACK_ALIGNMENT);
	S->sp += STACK_ALIGNMENT;
	for (uint32 i = 0; i < bytes; i++) {
		S->memory[S->sp - i] = S->code[S->ip++]; 
	}
}

/*	function:	spy_pushInt
 *
 *	desc:		pushes a 64 bit integer @value onto the stack
 *
 *	returns:	nothing
 */
static inline
void
spy_pushInt(spy_state* S, uint64 value) {
	S->sp += SIZEOF_INT;
	memcpy(&S->memory[S->sp - SIZEOF_INT + 1], &value, SIZEOF_INT); 
}

/*	function:	spy_popInt
 *
 *	desc:		pops a 64 bit integer from the stack and returns
 *				it to the caller
 *
 *	returns:	64 bit integer from the stack
 */
static inline
uint64
spy_popInt(spy_state* S) {
	uint64 tos;
	memcpy(&tos, &S->memory[S->sp - SIZEOF_INT + 1], SIZEOF_INT);
	S->sp -= SIZEOF_INT;
	return tos;	
}

/*	function:	spy_popFloat
 *	
 *	desc:		pops a 64 bit floating point number from the stack
 *				and returns it to the caller
 *
 *	returns:	64 bit float from the stack
 */
static inline
float64
spy_popFloat(spy_state* S) {
	float64 tos;
	memcpy(&tos, &S->memory[S->sp - SIZEOF_FLOAT + 1], SIZEOF_FLOAT);
	S->sp -= SIZEOF_FLOAT;
	return tos;	
}

/*	function:	spy_readInt8
 *
 *	desc:		reads one byte from code memory (starting at S->ip)
 *				and returns it to the caller... note that this does
 *				NOT read from the stack
 *
 *	returns:	8 bit integer from code memory
 */
static inline
uint8
spy_readInt8(spy_state* S) {
	return S->code[S->ip++];
}

/*	function:	spy_readInt16
 *
 *	desc:		reads two bytes from code memory (starting at S->ip)
 *				and returns it to the caller... note that this does
 *				NOT read from the stack
 *
 *	returns:	16 bit integer from code memory
 */
static inline
uint16
spy_readInt16(spy_state* S) {
	uint16 result;
	memcpy(&result, &S->code[S->ip], 2);
	S->ip += 2;
	return bswap_16(result);
}

/*	function:	spy_readInt32
 *
 *	desc:		reads four bytes from code memory (starting at S->ip)
 *				and returns it to the caller... note that this does
 *				NOT read from the stack
 *
 *	returns:	32 bit integer from code memory
 */
static inline
uint32
spy_readInt32(spy_state* S) {
	uint32 result;
	memcpy(&result, &S->code[S->ip], 4);
	S->ip += 4;
	return bswap_32(result);
}

/*	function:	spy_readInt64
 *
 *	desc:		reads eight bytes from code memory (starting at S->ip)
 *				and returns it to the caller... note that this does
 *				NOT read from the stack
 *
 *	returns:	64 bit integer from code memory
 */
static inline
uint64
spy_readInt64(spy_state* S) {
	uint64 result;
	memcpy(&result, &S->code[S->ip], 8);
	S->ip += 8;
	return bswap_64(result);
}

/* function:	spy_dumpStack
 *
 * desc:		for debugging purposes, prints out the stack
 *
 * returns:		nothing
 */
static
void
spy_dumpStack(spy_state* S) {
	printf("STACK DUMP:\n");
	for (uint32 i = START_STACK + 1; i <= S->sp; i++) {
		printf("0x%04x: %02x\n", i, S->memory[i]);	
	}
}

/* function:	spy_run
 *
 * desc:		executes Spyre bytecode passed in @code.  @size
 *				is the length of the bytecode array.
 */				
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
		&&btoi,		&&call,		&&ret_i,	&&ret_f
	};

	/* allocate space for S->code to store the code */
	if (S->code) free(S->code);
	S->code = malloc(size + 1);
	
	/* copy the argument code into S->code */
	memcpy(S->code, code, size);
	S->code[size] = 0;

	uint8 opcode;

	/* some variables that may be used by each instruction */
	uint8	a8, b8;
	uint16	a16, b16;
	uint32	a32, b32;
	uint64	a64, b64;

	while ((opcode = S->code[S->ip++])) {
		
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
		spy_pushInt(S, spy_popInt(S) + spy_popInt(S));
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
btoi:
		/* for now do nothing... stack is already aligned to 8 bytes
		 * so there is no need to cast from a byte to an int
		 */
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
		S->ip = a32;

		continue;
ret_i:
		continue;
ret_f:
	
		continue;
		
	}	

	spy_dumpStack(S);

}
