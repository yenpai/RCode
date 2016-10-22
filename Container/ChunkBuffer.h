#ifndef CHUNK_BUFFER_H_
#define CHUNK_BUFFER_H_

#include <stdint.h>

typedef struct ChunkBuffer ChunkBuffer;

struct ChunkBuffer {
	int       (* const Append)      (ChunkBuffer *, uint8_t * ptr,  uint32_t size);
	int       (* const Delete)      (ChunkBuffer *, uint32_t index, uint32_t size);
	uint8_t * (* const GetBufPtr)   (ChunkBuffer *);
	uint32_t  (* const GetCurSize)  (ChunkBuffer *);
	uint32_t  (* const GetMaxSize)  (ChunkBuffer *);
	void      (* const Destroy)     (ChunkBuffer *);
};

ChunkBuffer * ChunkBufferCreate(uint32_t exp_size, uint32_t max_size);

#endif
