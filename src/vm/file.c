#include <threads/malloc.h>
#include <stdio.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "vm/swap.h"
#include "vm/file.h"

void lru_list_init(void)
{
    list_init(&lru_list);
    lock_init(&lru_list_lock);
    lru_clock = NULL;
}

void add_page_to_lru_list(struct page *page)
{
    if(page == NULL)
        return;
    lock_acquire(&lru_list_lock);
    list_push_back(&lru_list, &page->lru);
    lock_release(&lru_list_lock);
}

void del_page_from_lru_list(struct page* page)
{
    if(page == NULL)
        return;
    if(lru_clock == page)
        lru_clock = list_entry(list_remove(&page->lru), struct page, lru);
    else
        list_remove(&page->lru);
}

struct page *alloc_page(enum palloc_flags flags)
{
    void *kaddr;
    struct page *new_page;
    if((flags & PAL_USER) == 0)
        return NULL;
    kaddr = palloc_get_page(flags);
    while(kaddr == NULL){
        try_to_free_pages();
        kaddr = palloc_get_page(flags);
    }
    new_page = malloc(sizeof(struct page));
    if(new_page == NULL){
        palloc_free_page(kaddr);
        return NULL;
    }
    new_page->pg_thread = thread_current();
    new_page->kaddr  = kaddr;
    add_page_to_lru_list(new_page);
    return new_page;
}

void free_page(void *kaddr)
{
    struct page *lru_page;
    lock_acquire(&lru_list_lock);
    for(struct list_elem *element = list_begin(&lru_list); element != list_end(&lru_list); element = list_next(element)){
        lru_page = list_entry(element, struct page, lru);
        if(lru_page->kaddr == kaddr){
            __free_page(lru_page);
            break;
        }
    }
    lock_release(&lru_list_lock);
}

void __free_page(struct page *page)
{
    palloc_free_page(page->kaddr);
    del_page_from_lru_list(page);
    free(page);
}

struct list_elem* get_next_lru_clock(void)
{
    struct list_elem *element;
    if(lru_clock == NULL){
        element = list_begin(&lru_list);
        if(element != list_end(&lru_list)){
            lru_clock = list_entry(element, struct page, lru);
            return element;
        }
        else
            return NULL;
    }
    element = list_next(&lru_clock->lru);
    if(element == list_end(&lru_list)){
        if(&lru_clock->lru == list_begin(&lru_list))
            return NULL;
        else
            element = list_begin(&lru_list);
    }
    lru_clock = list_entry(element, struct page, lru);
    return element;
}

void try_to_free_pages(void)
{
    struct page *lru_page;
    struct thread *page_thread;
    struct list_elem *element;
    lock_acquire(&lru_list_lock);
    if(list_empty(&lru_list) == true){
        lock_release(&lru_list_lock);
        return;
    }
    while(1){
        element = get_next_lru_clock();
        if(element == NULL){
            lock_release(&lru_list_lock);
            return;
        }
        lru_page = list_entry(element, struct page, lru);
        if(lru_page->vme->pinned == true)
            continue;
        page_thread = lru_page->pg_thread;
        if(pagedir_is_accessed(page_thread->pagedir, lru_page->vme->vaddr)){
            pagedir_set_accessed(page_thread->pagedir, lru_page->vme->vaddr, false);
            continue;
        }
        if(pagedir_is_dirty(page_thread->pagedir, lru_page->vme->vaddr) || lru_page->vme->type == VM_ANON){
            if(lru_page->vme->type == VM_FILE){
                lock_acquire(&file_lock);
                file_write_at(lru_page->vme->file, lru_page->kaddr ,lru_page->vme->read_bytes, lru_page->vme->offset);
                lock_release(&file_lock);
            }
            else{
                lru_page->vme->type = VM_ANON;
                lru_page->vme->swap_slot = swap_out(lru_page->kaddr);
            }
        }
        lru_page->vme->is_loaded = false;
        pagedir_clear_page(page_thread->pagedir, lru_page->vme->vaddr);
        __free_page(lru_page);
        break;
    }
    lock_release(&lru_list_lock);
    return;
}
