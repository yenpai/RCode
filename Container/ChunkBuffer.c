#include <stdlib.h>
#include <string.h>
#include "ChunkBuffer.h"

typedef struct MyChunkBuffer MyChunkBuffer;

struct MyChunkBuffer {

	ChunkBuffer self;

	uint32_t    cur_size; /* current data size */
	uint32_t    buf_size; /* current buffer total size */
	uint32_t    max_size; /* allow expand max size */
	uint32_t    exp_size; /* expand size when buffer size not enought */
	uint8_t  *  buffer;
};

static int M_Append (ChunkBuffer * self, uint8_t * ptr, uint32_t size)
{
	MyChunkBuffer * this = (MyChunkBuffer *) self;

	uint32_t  copy_size   = 0;
	uint32_t  remain_size = 0;
	uint32_t  new_size    = 0;
	uint8_t * new_buffer  = NULL;

	if (size == 0 || ptr == NULL)
		return -1;

	if (this->cur_size == this->max_size)
		return -2;

	remain_size = this->buf_size - this->cur_size;
	new_size    = this->buf_size;

	if (size > remain_size)
	{
		new_size += this->exp_size;
		while (new_size <= this->max_size)
		{
			remain_size += this->exp_size;
			if (size <= remain_size)
				break;
			new_size    += this->exp_size;
		}

		if (new_size > this->max_size)
			new_size = this->max_size;
	}

	/* Expend buffer size */
	if (new_size > this->buf_size)
	{
		new_buffer = realloc(this->buffer, new_size);
		if (new_buffer != this->buffer)
		{
			memcpy(new_buffer, this->buffer, this->cur_size);
			free(this->buffer);
		}

		this->buffer   = new_buffer;
		this->buf_size = new_size;
	}

	copy_size = (size > (this->buf_size - this->cur_size)) ?
		this->buf_size - this->cur_size : size ;

	memcpy(this->buffer + this->cur_size, ptr, copy_size);
	this->cur_size += copy_size;
	
	return (copy_size == size) ? 0 : -3;
}

static int M_Delete (ChunkBuffer * self, uint32_t index, uint32_t size)
{
	MyChunkBuffer * this = (MyChunkBuffer *) self;

	uint32_t num;

	if (size == 0)
		return -1;

	if (this->cur_size < (index + size))
		return -2;

	for (num = index ; num < (this->cur_size - size) ; num++)
		this->buffer[num] = this->buffer[num+size];

	this->cur_size -= size;
	return 0;
}

static uint8_t * M_GetBufPtr(ChunkBuffer * self)
{
	return ((MyChunkBuffer *) self)->buffer;
}

static uint32_t M_GetCurSize(ChunkBuffer * self)
{
	return ((MyChunkBuffer *) self)->cur_size;
}

static uint32_t M_GetMaxSize(ChunkBuffer * self)
{
	return ((MyChunkBuffer *) self)->max_size;
}

static void M_Destroy (ChunkBuffer * self)
{
	MyChunkBuffer * this = (MyChunkBuffer *) self;

	if (this->buffer)
		free(this->buffer);

	free(this);
}

ChunkBuffer * ChunkBufferCreate(uint32_t exp_size, uint32_t max_size)
{
	MyChunkBuffer * this = NULL;
	ChunkBuffer     self = {
		.Append     = M_Append,
		.Delete     = M_Delete,
		.GetBufPtr  = M_GetBufPtr,
		.GetCurSize = M_GetCurSize,
		.GetMaxSize = M_GetMaxSize,
		.Destroy    = M_Destroy,
	};
	
	if (max_size == 0 || exp_size == 0)
		return NULL;

	if ((this = malloc(sizeof(MyChunkBuffer))) == NULL)
		return NULL;

	memcpy(&this->self, &self, sizeof(ChunkBuffer));
	this->cur_size = 0;
	this->buf_size = 0;
	this->max_size = max_size;
	this->exp_size = exp_size;
	this->buffer   = NULL;

	return &this->self;
}
