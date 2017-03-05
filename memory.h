#ifndef __MEMORY_H__
#define __MEMORY_H__

struct Chunk_Footer {
	char *base;
	char *block_frontier;
	Chunk_Footer *next;
};

struct Block_Footer {
	size_t capacity;
	size_t nbytes_used;
	Block_Footer *next;
	Block_Footer *prev;
};

struct Arena_Address {
	char *byte_addr;
	Block_Footer *block_footer;
};

struct Free_Entry {
	size_t size;
	Free_Entry *next;
};

struct Entry_Header {
	size_t size;
	Entry_Header *next;
	Entry_Header *prev;
};

struct Memory_Arena {
	Free_Entry *entry_free_head;
	Entry_Header *last_entry;
	Block_Footer *base;
	Block_Footer *active_block;
};

#endif
