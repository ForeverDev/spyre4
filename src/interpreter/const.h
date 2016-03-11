#ifndef CONST_H
#define CONST_H

/* load correct byteswapping header */
#ifdef __linux__
#include <byteswap.h>
#define bswap_16 __bswap_16
#define bswap_32 __bswap_32
#define bswap_64 __bswap_64
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define bswap_16 OSSwapInt16
#define bswap_32 OSSwapInt32
#define bswap_64 OSSwapInt64
#else
#include <sys/endian.h>
#define bswap_16 bswap16
#define bswap_32 bswap32
#define bswap_64 bswap64
#endif

#define SIZE_MEMORY		0x100000
#define SIZE_STACK		0x005000
#define SIZE_ROM		0x005000

#define START_ROM		0x000000
#define START_STACK		(START_ROM + SIZE_ROM)
#define START_HEAP		(START_STACK + SIZE_STACK)

#define STACK_ALIGNMENT	8

#define SIZEOF_BYTE		1
#define SIZEOF_INT		8
#define SIZEOF_FLOAT	8

typedef char				int8;
typedef short				int16;
typedef int					int32;
typedef long long			int64;

typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned int		uint32;
typedef unsigned long long	uint64;

typedef float				float32;
typedef double				float64;

#endif
