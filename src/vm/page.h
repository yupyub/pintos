#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#define VM_BIN 1 
#define VM_FILE 2
#define VM_ANON 3
#define CLOSE_ALL 9999
struct vm_entry{
	uint8_t type;                      // VM_BIN, VM_FILE, VM_ANON
	void *vaddr;                       // virtual address 
	bool writable;                     
	bool is_loaded;                    // if true, physical memory is loaded
	bool pinned;
	struct file *file;
	struct list_elem mmap_elem;        // list_elem for mmap_file's vm_list
	size_t offset;
	size_t read_bytes;                   
	size_t zero_bytes;
	size_t swap_slot;
	struct hash_elem elem;             // hash elem for thread's vm
};

struct page{
	void *kaddr;
	struct vm_entry *vme;
	struct thread *pg_thread;
	struct list_elem lru;
};

void vm_init(struct hash *vm);
void vm_destroy(struct hash *vm);
struct vm_entry *find_vme(void *vaddr);
struct vm_entry *find_vme(void *vaddr);
bool insert_vme(struct hash *vm, struct vm_entry *vme);
bool delete_vme(struct hash *vm, struct vm_entry *vme);
bool insert_vme(struct hash *vm, struct vm_entry *vme);
bool delete_vme(struct hash *vm, struct vm_entry *vme);
bool load_file(void *kaddr, struct vm_entry *vme);
#endif
