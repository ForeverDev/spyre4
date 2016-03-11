#ifndef SPYRE_H
#define SPYRE_H

#include <stddef.h>
#include "const.h"

typedef struct spy_state		spy_state;
typedef struct spy_memoryNode	spy_memoryNode;
typedef struct spy_memoryMap	spy_memoryMap;

struct spy_state {
	
	spy_memoryMap*		heap_data;
	uint32				ip;
	uint32				sp;
	uint32				bp;
	uint8*				code;
	uint8				memory[SIZE_MEMORY];

};

struct spy_memoryNode {

	uint32					size;
	uint8*					start;
	spy_memoryNode*			next;
	spy_memoryNode*			prev;

};

struct spy_memoryMap {
	
	spy_memoryNode*			head;
	spy_memoryNode*			tail;
	uint32					size;

};

spy_state*				spy_newstate(); 
void					spy_run(spy_state*, const uint8*, size_t);

static inline void		spy_pushFromCode(spy_state*, size_t);
static inline void		spy_pushInt(spy_state*, uint64);
static inline void		spy_pushFloat(spy_state*, float64);
static inline uint64	spy_popInt(spy_state*);
static inline float64	spy_popFloat(spy_state*);
static inline uint8		spy_readInt8(spy_state*);
static inline uint16	spy_readInt16(spy_state*);
static inline uint32	spy_readInt32(spy_state*);
static inline uint64	spy_readInt64(spy_state*);

static void				spy_dumpStack(spy_state*);

#endif
