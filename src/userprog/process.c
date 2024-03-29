#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads/malloc.h>
#include "threads/flags.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "vm/swap.h"
#include "vm/file.h"
static thread_func start_process NO_RETURN;
static bool install_page (void *upage, void *kpage, bool writable);
static bool load (const char *cmdline, void (**eip) (void), void **esp);
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
    tid_t
process_execute (const char *file_name) 
{ 
    char *fn_copy;
    tid_t tid;

    int file_name_size = strlen(file_name)+1;
    char program_name[file_name_size];      
    char *save_ptr=NULL;

    fn_copy = palloc_get_page (0);
    if (fn_copy == NULL)
        return TID_ERROR;
    strlcpy (fn_copy, file_name, PGSIZE);

    strlcpy(program_name,file_name,file_name_size);
    strtok_r(program_name," ",&save_ptr);

    tid = thread_create (program_name, PRI_DEFAULT, start_process,fn_copy);
    if (tid == TID_ERROR)
        palloc_free_page (fn_copy); 

    return tid;
}

struct file *process_get_file(int fd)
{
    struct file *pReturn;
    if(fd < thread_current()->next_fd){
        pReturn = thread_current()->file_descriptor[fd];
        return pReturn;
    }
    else
        return NULL;
}
    void
process_close_file(int fd)
{
    struct file *delete_file;
    delete_file = process_get_file(fd);
    if(delete_file != NULL){
        file_close(delete_file);
        thread_current()->file_descriptor[fd] = NULL;
    }
}

    int
process_add_file(struct file *f)
{
    struct file **fd = NULL;

    int nextFd = thread_current()->next_fd;
    fd = thread_current()->file_descriptor;

    if(fd != NULL){
        fd[nextFd] = f;
        thread_current()->next_fd = thread_current()->next_fd+1;
        return nextFd;
    }
    else
        return -1;
}

void argument_stack(char **parse,int count,void **esp)
{
    int parse_count=0;             // sum of arguments length;
    unsigned int argv_address;     // argv[0] address
    unsigned int address[count];   // argument address

    if(parse==NULL)
        return;

    for(int i=count-1;i>=0;i--){ 
        for(int j=strlen(parse[i]);j>=0;j--){
            *esp=*esp-1;
            **(char **)esp=parse[i][j];
            parse_count++;       //count length of string that we pushed
        }
        address[i]=*(unsigned int *)esp; //store address of argument
    }
    /*Word Align*/
    for(int i=0;i<4-(parse_count%4);i++){
        *esp=*esp-1;
        **(uint8_t **)esp=0;
    }

    /*Last argv*/
    *esp=*esp-4;
    **(char* **)esp=0;

    /*Push Argument Address*/
    for(int i=count-1;i>=0;i--){
        *esp=*esp-4;
        **(char* **)esp=(char *)address[i];
    }

    /* Main argv */
    argv_address=*(unsigned int *)esp;
    *esp=*esp-4;
    **(char* **)esp=(char *)argv_address;
    /* Main argc */

    *esp=*esp-4;
    **(int **)esp=count;

    /* Fake Address */
    *esp=*esp-4;
    **(int **)esp=0;
}

/* get_child_process */
struct thread *get_child_process(int pid)
{
    struct list *child_list=&(thread_current()->child_list);
    struct list_elem *element;
    struct thread *child_process = NULL;

    /* find child process and return */
    for(element=list_begin(child_list); element != list_end(child_list);            		element=list_next(element)){
        child_process = list_entry(element,struct thread,childelem);
        if(child_process->tid == pid)
            return child_process;
    }
    return NULL;     
}
/* remove_child_process */
    void
remove_child_process(struct thread *cp)
{
    if(cp != NULL){
        list_remove(&(cp->childelem));
        palloc_free_page(cp);
    }
}

bool expand_stack(void *addr)
{
    struct vm_entry *vme;
    struct page *stack_page;

    if((size_t)(PHYS_BASE - pg_round_down(addr)) > MAX_STACK_SIZE)
        return false;

    vme = malloc(sizeof(struct vm_entry));
    if(vme == NULL)
        return false;
    vme->vaddr     = pg_round_down(addr);
    vme->type      = VM_ANON;
    vme->writable  = true;
    vme->pinned    = true;
    vme->is_loaded = true;
    stack_page = alloc_page(PAL_USER);
    if(stack_page == NULL){
        free(vme);
        return false;
    }
    stack_page->vme = vme;
    if(install_page(vme->vaddr, stack_page->kaddr, vme->writable) == false){
        free_page(stack_page);
        free(vme);
        return false;
    }
    if(insert_vme(&thread_current()->vm, vme) == false){
        free_page(stack_page);
        free(vme);
        return false;
    }
    if(intr_context())
        vme->pinned = false;

    return true;
}

/* A thread function that loads a user process and starts it
   running. */
    static void
start_process (void *file_name_)
{
    char *file_name = file_name_;
    bool success;
    struct intr_frame if_;

    char *parse[LOADER_ARGS_LEN/2 + 1];
    char *token;
    char *save_ptr;
    int count=0;

    vm_init(&(thread_current()->vm));
    for(token=strtok_r(file_name," ",&save_ptr); token!=NULL; token=strtok_r(NULL," ",&save_ptr)){	 	
        parse[count] = token;
        count++;  
    }

    memset (&if_, 0, sizeof if_);
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;
    success = load (parse[0], &if_.eip, &if_.esp);

    thread_current()->load_success=success;
    if(success)
        argument_stack(parse,count,&if_.esp);
    sema_up(&(thread_current()->load_semaphore));

    palloc_free_page (file_name);
    if (!success) 
        thread_exit();


    /* Start the user process by simulating a return from an
       interrupt, implemented by intr_exit (in
       threads/intr-stubs.S).  Because intr_exit takes all of its
       arguments on the stack in the form of a `struct intr_frame',
       we just point the stack pointer (%esp) to our stack frame
       and jump to it. */
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
    NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */

/* process_wait untill child process finish */
    int
process_wait (tid_t child_tid) 
{
    int status;
    struct thread *child_process = NULL;
    child_process = get_child_process(child_tid);

    if(child_process == NULL)
        return -1;

    sema_down(&(child_process->exit_semaphore));
    status = child_process->process_exit_status;
    remove_child_process(child_process);

    return status;
}

/* Free the current process's resources. */
    void
process_exit (void)
{
    struct thread *cur = thread_current ();
    uint32_t *pd;

    /* close file and file descriptor delete */
    for(int i=2; i<cur->next_fd; i++)
        process_close_file(i);
    palloc_free_page(cur->file_descriptor);
    /* unmap the all process's mapped file and 
       destroy vm_entry hash */
    vm_destroy(&cur->vm);
    /* Destroy the current process's page directory and switch back
       to the kernel-only page directory. */
    pd = cur->pagedir;
    if (pd != NULL){
        cur->pagedir = NULL;
        pagedir_activate (NULL);
        pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
    void
process_activate (void)
{
    struct thread *t = thread_current ();

    /* Activate thread's page tables. */
    pagedir_activate (t->pagedir);

    /* Set thread's kernel stack for use in processing
       interrupts. */
    tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
        uint32_t read_bytes, uint32_t zero_bytes,
        bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
    bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
    struct thread *t = thread_current ();
    struct Elf32_Ehdr ehdr;
    struct file *file = NULL;
    off_t file_ofs;
    bool success = false;
    int i;

    /* Allocate and activate page directory. */
    t->pagedir = pagedir_create ();
    if (t->pagedir == NULL) 
        goto done;
    process_activate ();

    /* Open executable file. */
    file = filesys_open (file_name);
    if (file == NULL) 
    {
        printf ("load: %s: open failed\n", file_name);
        goto done; 
    }

    /* Read and verify executable header. */
    if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
            || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
            || ehdr.e_type != 2
            || ehdr.e_machine != 3
            || ehdr.e_version != 1
            || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
            || ehdr.e_phnum > 1024) 
    {
        printf ("load: %s: error loading executable\n", file_name);
        goto done; 
    }

    /* Read program headers. */
    file_ofs = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++) 
    {
        struct Elf32_Phdr phdr;

        if (file_ofs < 0 || file_ofs > file_length (file))
            goto done;
        file_seek (file, file_ofs);

        if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
            goto done;
        file_ofs += sizeof phdr;
        switch (phdr.p_type) 
        {
            case PT_NULL:
            case PT_NOTE:
            case PT_PHDR:
            case PT_STACK:
            default:
                /* Ignore this segment. */
                break;
            case PT_DYNAMIC:
            case PT_INTERP:
            case PT_SHLIB:
                goto done;
            case PT_LOAD:
                if (validate_segment (&phdr, file)) 
                {
                    bool writable = (phdr.p_flags & PF_W) != 0;
                    uint32_t file_page = phdr.p_offset & ~PGMASK;
                    uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
                    uint32_t page_offset = phdr.p_vaddr & PGMASK;
                    uint32_t read_bytes, zero_bytes;
                    if (phdr.p_filesz > 0)
                    {
                        /* Normal segment.
                           Read initial part from disk and zero the rest. */
                        read_bytes = page_offset + phdr.p_filesz;
                        zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                    }
                    else 
                    {
                        /* Entirely zero.
                           Don't read anything from disk. */
                        read_bytes = 0;
                        zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                    }
                    if (!load_segment (file, file_page, (void *) mem_page,
                                read_bytes, zero_bytes, writable))
                        goto done;
                }
                else
                    goto done;
                break;
        }
    }

    /* Set up stack. */
    if (!setup_stack (esp))
        goto done;

    /* Start address. */
    *eip = (void (*) (void)) ehdr.e_entry;

    success = true;

done:
    /* We arrive here whether the load is successful or not. */
    file_close(file);
    return success;
}
/* load() helpers. */


/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
    static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
    if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
        return false; 
    if (phdr->p_memsz < phdr->p_filesz) 
        return false; 
    if (phdr->p_offset > (Elf32_Off) file_length (file)) 
        return false;
    if (phdr->p_memsz == 0)
        return false;
    if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
        return false;
    if (phdr->p_vaddr < PGSIZE)
        return false;
    if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
        return false;
    if (!is_user_vaddr ((void *) phdr->p_vaddr))
        return false;

    return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

   - READ_BYTES bytes at UPAGE must be read from FILE
   starting at offset OFS.

   - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
    static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
        uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
    struct file *reopen_file = file_reopen(file);
    struct vm_entry *vme;
    ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT (ofs % PGSIZE == 0);
    ASSERT (pg_ofs (upage) == 0);

    file_seek (file, ofs);
    while (read_bytes > 0 || zero_bytes > 0){
        vme = malloc(sizeof(struct vm_entry));
        if(vme == NULL)
            return false;
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        vme->file = reopen_file;
        vme->read_bytes = page_read_bytes;
        vme->zero_bytes = page_zero_bytes;
        vme->writable = writable;
        vme->vaddr = upage;
        vme->type = VM_BIN;
        vme->offset = ofs;
        vme->pinned = false;
        vme->is_loaded = false;

        if(insert_vme(&thread_current()->vm, vme) == false ){
            printf("insert_vme error!\n");
        }
        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += page_read_bytes;
    }
    return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
    bool
setup_stack (void **esp) 
{
    struct page *kpage;
    bool success = false;
    struct vm_entry *vme;	
    void *virtual_address = ((uint8_t *) PHYS_BASE) - PGSIZE;
    kpage = alloc_page (PAL_USER | PAL_ZERO);
    if (kpage != NULL){
        success = install_page ( pg_round_down(virtual_address), kpage->kaddr, true);
        if (success)
            *esp = PHYS_BASE;
        else
            free_page (kpage->kaddr);
    }

    vme = malloc(sizeof(struct vm_entry));
    if(vme == NULL){
        free_page(kpage);
        return false;
    }

    vme->vaddr = pg_round_down(virtual_address);
    vme->type = VM_ANON;
    kpage->vme = vme;
    vme->pinned = true;
    vme->is_loaded = true;
    vme->writable = true;

    success = insert_vme(&thread_current()->vm, vme);

    return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
    static bool
install_page (void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current ();

    /* Verify that there's not already a page at that virtual
       address, then map our page there. */
    return (pagedir_get_page (t->pagedir, upage) == NULL
            && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

bool handle_mm_fault(struct vm_entry *vme)
{
    struct page *new_page = alloc_page(PAL_USER);
    new_page->vme = vme;
    vme->pinned = true;
    if(vme->is_loaded == true){       // if vme is already loaded, return false
        free_page(new_page);
        return false;
    }
    if(new_page == NULL)
        return false;
    switch(vme->type){
        case VM_BIN:
            if(load_file(new_page->kaddr,vme) == false){
                free_page(new_page->kaddr);
                return false;
            }
            break;
        case VM_FILE:
            if(load_file(new_page->kaddr,vme) == false){
                free_page(new_page->kaddr);
                return false;
            }
            break;
        case VM_ANON:
            swap_in(vme->swap_slot, new_page->kaddr);
            break;
        default:
            return false;
    }
    if(install_page(vme->vaddr,new_page->kaddr, vme->writable) == false){
        free_page(new_page->kaddr);
        return false;
    }
    vme->is_loaded = true;
    return true;
}
