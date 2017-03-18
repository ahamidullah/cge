size_t BLOCK_DATA_SIZE = (platform_get_page_size() * 256) - sizeof(Block_Footer);
size_t BLOCK_DATA_PLUS_FOOTER_SIZE = BLOCK_DATA_SIZE + sizeof(Block_Footer);
size_t CHUNK_DATA_SIZE = BLOCK_DATA_PLUS_FOOTER_SIZE * 1024;
size_t CHUNK_DATA_PLUS_FOOTER_SIZE = CHUNK_DATA_SIZE + sizeof(Chunk_Footer);

constexpr int round_up(float f);

Chunk_Footer *
mem_make_chunk()
{
	void *chunk = platform_get_memory(CHUNK_DATA_PLUS_FOOTER_SIZE);
	Chunk_Footer *footer = (Chunk_Footer *)((char *)chunk + CHUNK_DATA_SIZE);
	footer->base = (char *)chunk;
	footer->block_frontier = footer->base;
	footer->next = NULL;
	return footer;
}

Chunk_Footer *g_mem_chunks = mem_make_chunk();
Chunk_Footer *g_active_chunk = g_mem_chunks;
Block_Footer *g_block_free_head = NULL;

inline char *
get_block_start(Block_Footer *f)
{
	return ((char *)f - f->capacity);
}

inline Block_Footer *
get_block_footer(char *start)
{
	return (Block_Footer *)((char *)start + BLOCK_DATA_SIZE);
}

Block_Footer *
mem_make_block()
{
	Block_Footer *blk;
	if (g_block_free_head) {
		blk = g_block_free_head;
		g_block_free_head = g_block_free_head->next;
	} else {
		if (((g_active_chunk->block_frontier + BLOCK_DATA_PLUS_FOOTER_SIZE) - g_active_chunk->base) > CHUNK_DATA_SIZE) {
			g_active_chunk->next = mem_make_chunk();
			g_active_chunk = g_active_chunk->next;
		}
		blk = get_block_footer(g_active_chunk->block_frontier);
		g_active_chunk->block_frontier += (BLOCK_DATA_PLUS_FOOTER_SIZE);
	}
	blk->capacity = BLOCK_DATA_SIZE;
	blk->nbytes_used = 0;
	blk->next = NULL;
	blk->prev = NULL;
	return blk;
}

inline Memory_Arena
mem_make_arena()
{
	Memory_Arena m;
	m.entry_free_head = NULL;
	m.last_entry = NULL;
	m.base = mem_make_block();
	m.active_block = m.base;
	return m;
}

void
free_block(Block_Footer *f)
{
	f->next = g_block_free_head;
	g_block_free_head = f;
}

void
mem_destroy_arena(const Memory_Arena *ma)
{
	for (Block_Footer *f = ma->base, *next = NULL; f; f = next) {
		next = f->next;
		free_block(f);
	}
	//ma->base = ma->active_block = NULL;
}

// Complicated, becuase we might have to swap bytes across blocks or chunks that are not adjacent.
// Some edge cases: start and end the same
//                  start pointer and end pointer both change blocks as the reverse ends
//                  end pointer is the first byte of a new block
//                  start and end in different chunks
// Do we even use this anymore?
void
mem_reverse(Arena_Address *start, Arena_Address *end, Memory_Arena *ma)
{
	assert(start->byte_addr >= get_block_start(start->block_footer) && start->byte_addr < (char *)start->block_footer);
	assert(end->byte_addr >= get_block_start(end->block_footer) && end->byte_addr < (char *)end->block_footer);

	// Since mem_end returns one byte past the last byte written, we want to move our end pointer back one so we're pointing to the last byte written.
	if (--end->byte_addr < get_block_start(end->block_footer)) { // Have we gone past the start of the block?
		end->block_footer = end->block_footer->prev;
		assert(end->block_footer);
		end->byte_addr = (char *)end->block_footer - 1;
	}

	bool swapped_places = false;
	while (start->byte_addr != end->byte_addr && !swapped_places) {
		char tmp = *start->byte_addr;
		*start->byte_addr = *end->byte_addr;
		*end->byte_addr = tmp;

		char *prev_end = end->byte_addr;
		if (++start->byte_addr >= (char *)start->block_footer) { // Have we gone off the end of the block?
			start->block_footer = start->block_footer->next;
			assert(start->block_footer);
			start->byte_addr = get_block_start(start->block_footer);
		}
		if (--end->byte_addr < get_block_start(end->block_footer)) { // Have we gone past the start of the block?
			end->block_footer = end->block_footer->prev;
			assert(end->block_footer);
			end->byte_addr = (char *)end->block_footer - 1;
		}
		swapped_places = start->byte_addr == prev_end;
	}
}

inline char *
get_entry_data(Entry_Header *e)
{
	return (char *)e + sizeof(Entry_Header);
}

inline Entry_Header *
get_entry_header(void *p)
{
	return (Entry_Header *)((char *)p - sizeof(Entry_Header));
}

void *
mem_start(Memory_Arena *ma)
{
	// TODO: It's actually not. Need to make it so that we don't get a block on startup incase the first allocation is bigger than one block.
	if (ma->last_entry) // Do we have any entries? If we do, it's guarenteed that the first one is right at the start of the base block.
		return get_entry_data((Entry_Header *)get_block_start(ma->base));
	return NULL;
}

void *
mem_next(void *p)
{
	return get_entry_header(p)->next;
}

bool
mem_has_elems(Memory_Arena *ma)
{
	return ma->last_entry;
}

void
mem_arena_add_block(Memory_Arena *ma)
{
	Block_Footer *new_blk = mem_make_block();
	new_blk->prev = ma->active_block;
	ma->active_block->next = new_blk;
	ma->active_block = new_blk;
}

#define mem_alloc(type, arena) (type *)mem_push(sizeof(type), arena)
#define mem_alloc_array(type, count, arena) (type *)mem_push_contiguous(sizeof(type)*count, arena)

// TODO: combine the block group logic here with mem_make_block and just call that.
void *
mem_push_contiguous(size_t size, Memory_Arena *ma)
{
	assert(size <= CHUNK_DATA_SIZE);
	size_t nblocks_needed = round_up((float)(size + sizeof(Block_Footer)) / BLOCK_DATA_PLUS_FOOTER_SIZE);

	// We could try to find our needed blocks among the free blocks, but for now we just take what we need from the block frontier of the chunk.
	// Is there enough space in our current chunk for the contiguous memory requested?
	if (g_active_chunk->block_frontier + (BLOCK_DATA_PLUS_FOOTER_SIZE*nblocks_needed) - g_active_chunk->base > CHUNK_DATA_SIZE) {
		// Add all of the unused blocks in the current chunk to the free list.
		while (g_active_chunk->block_frontier + BLOCK_DATA_PLUS_FOOTER_SIZE - g_active_chunk->base > CHUNK_DATA_SIZE) {
			free_block(get_block_footer(g_active_chunk->block_frontier));
			g_active_chunk->block_frontier += BLOCK_DATA_PLUS_FOOTER_SIZE;
		}
		g_active_chunk->next = mem_make_chunk();
		g_active_chunk = g_active_chunk->next;
	}
	// Move us forward to the last block, where we will keep the footer for the enitre block group.
	Block_Footer *blk = get_block_footer(g_active_chunk->block_frontier + ((nblocks_needed - 1) * BLOCK_DATA_PLUS_FOOTER_SIZE));
	blk->capacity = nblocks_needed*BLOCK_DATA_PLUS_FOOTER_SIZE - sizeof(Block_Footer);
	blk->nbytes_used = size;
	blk->prev = ma->active_block;
	blk->next = NULL;
	ma->active_block->next = blk;
	ma->active_block = blk;
	g_active_chunk->block_frontier += BLOCK_DATA_PLUS_FOOTER_SIZE*nblocks_needed;
	return get_block_start(blk);
}

// TODO: Keep track of the largest free entry available to speed up allocation?
void *
mem_push(size_t size, Memory_Arena *ma)
{
	size_t size_with_header = size + sizeof(Entry_Header);
	assert(size_with_header <= BLOCK_DATA_SIZE);
	/*
	for (Free_Entry *f = ma->entry_free_head, *prev = NULL; f; prev = f, f = f->next) {
		// TODO: Re-add the remaining free entry space to the free entry list.
		if (size <= f->size) {
			// THIS IS BUSTED.
			if (prev)
				prev->next = f->next;
			return (char *)f + sizeof(Entry_Header);
		}
	}
	*/
	Entry_Header *new_entry_header;
	if (size_with_header <= (ma->active_block->capacity - ma->active_block->nbytes_used))
		new_entry_header = (Entry_Header *)(get_block_start(ma->active_block) + ma->active_block->nbytes_used);
	else { // Need a new block.
		mem_arena_add_block(ma);
		new_entry_header = (Entry_Header *)get_block_start(ma->active_block);
	}

	char *new_entry = get_entry_data(new_entry_header);
	new_entry_header->size = size;
	new_entry_header->next = NULL;
	new_entry_header->prev = ma->last_entry;
	if (ma->last_entry) {
		Entry_Header *last_entry_header = get_entry_header(ma->last_entry);
		last_entry_header->next = new_entry;
	}

	ma->last_entry = new_entry;
	ma->active_block->nbytes_used += size_with_header;
	// If we fill up the block exactly, we get a new one right away.
	if (ma->active_block->nbytes_used == ma->active_block->capacity)
		mem_arena_add_block(ma);
	return new_entry;
}

void
mem_free(Memory_Arena *ma, void *p)
{
	assert(p >= g_active_chunk->base + sizeof(Entry_Header) && p < g_active_chunk);
	// TODO: Merge adjacent free entries.
	Free_Entry *f = (Free_Entry *)get_entry_header(p);
	f->next = ma->entry_free_head;
	ma->entry_free_head = f;
}
