#include "lib/kernel/bitmap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "vm/file.h"
#include "vm/page.h"
#include "vm/swap.h"

struct bitmap *swap_map;
struct block *swap_block;
struct lock swap_lock;

void swap_init(void)
{
	swap_block = block_get_role(BLOCK_SWAP);
	if(swap_block == NULL)
		return;
	swap_map = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE );
	if(swap_map == NULL)
		return;
	bitmap_set_all(swap_map, SWAP_FREE);
	lock_init(&swap_lock);
}

void swap_in(size_t used_index, void* kaddr)
{
	lock_acquire(&swap_lock);
	if(bitmap_test(swap_map, used_index) == SWAP_FREE)
		return;
	for(int i=0; i<SECTORS_PER_PAGE; i++)
		block_read(swap_block, used_index * SECTORS_PER_PAGE + i, (uint8_t *)kaddr + i * BLOCK_SECTOR_SIZE);
	bitmap_flip(swap_map, used_index);
	lock_release(&swap_lock);
}

size_t swap_out(void *kaddr)
{
	size_t free_index;
	lock_acquire(&swap_lock);

	free_index = bitmap_scan_and_flip(swap_map, 0, 1, SWAP_FREE);
	if(free_index == BITMAP_ERROR)
		return BITMAP_ERROR;
	for(int i=0; i<SECTORS_PER_PAGE; i++)
		block_write(swap_block, free_index * SECTORS_PER_PAGE + i, (uint8_t *)kaddr + i * BLOCK_SECTOR_SIZE);
	lock_release(&swap_lock);
	return free_index;
}
