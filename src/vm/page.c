#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads/malloc.h>
#include <threads/palloc.h>
#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/file.h"

static void vm_destroy_func(struct hash_elem *e, void *aux UNUSED);
static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
static unsigned vm_hash_func(const struct hash_elem *e, void *aux UNUSED);

/* if a's vm_entry adress is less than b's vm_entry address return true */
static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
	if(hash_entry(a, struct vm_entry, elem)->vaddr < hash_entry(b, struct vm_entry, elem)->vaddr)
		return true;
	else 
		return false;
}

/* define hash function */
static unsigned vm_hash_func(const struct hash_elem *e, void *aux UNUSED)
{
	return hash_int((int)(hash_entry(e,struct vm_entry,elem)->vaddr));
}

static void vm_destroy_func(struct hash_elem *e, void *aux UNUSED)
{
	void *physical_address;
	struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
	if(vme->is_loaded == true){
		physical_address = pagedir_get_page(thread_current()->pagedir, vme->vaddr);
		free_page(physical_address);
		pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
	}
	free(vme);
}

void vm_init(struct hash *vm)
{
	hash_init(vm, vm_hash_func, vm_less_func, NULL);
}

void vm_destroy(struct hash *vm)
{
	hash_destroy(vm, vm_destroy_func);
}

struct vm_entry *find_vme(void *vaddr)
{
	struct hash_elem *element;
	struct vm_entry vme;
	vme.vaddr = pg_round_down(vaddr);
	element = hash_find(&thread_current()->vm, &vme.elem);
	if(element != NULL)
		return hash_entry(element, struct vm_entry, elem);
	return NULL;
}

bool insert_vme(struct hash *vm, struct vm_entry *vme)
{
	bool result = false;
	if(hash_insert(vm, &vme->elem) == NULL)
		result = true;
	return result;
}

bool delete_vme(struct hash *vm, struct vm_entry *vme)
{
	bool result = false;
	if(hash_delete(vm, &vme->elem) != NULL)
		result = true;
	free(vme);
	return result;   
}

bool load_file(void *kaddr, struct vm_entry *vme)
{
	if((int)vme->read_bytes == file_read_at(vme->file, kaddr, vme->read_bytes, vme->offset)){
		memset(kaddr + vme->read_bytes, 0, vme->zero_bytes);
        return true;
	} 
	return false;
}
