#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
#include <devices/shutdown.h>
#include <filesys/file.h>
#include <userprog/process.h>
#include <devices/input.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);

    void
syscall_init (void) 
{
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

    static void
syscall_handler (struct intr_frame *f ) 
{
    int arg[5];
    void *esp = f->esp;
    check_address(esp, f->esp);
    int syscall_num = *(int *)esp;
    switch(syscall_num){
        case SYS_HALT:
            halt();
            break;
        case SYS_EXIT:
            get_argument(esp,arg,1);
            exit(arg[0]);
            break;
        case SYS_EXEC:
            get_argument(esp,arg,1);
            check_valid_string((const void *)arg[0], f->esp);
            f->eax = exec((const char *)arg[0]);
            unpin_string((void *)arg[0]);
            break;
        case SYS_WAIT:
            get_argument(esp,arg,1);
            f->eax = wait(arg[0]);
            break;
        case SYS_CREATE:
            get_argument(esp,arg,2);
            check_valid_string((const void *)arg[0], f->esp);
            f->eax = create((const char *)arg[0],(unsigned)arg[1]);
            unpin_string((void *)arg[0]);
            break;
        case SYS_REMOVE:
            get_argument(esp,arg,1);
            check_valid_string((const void *)arg[0], f->esp);
            f->eax=remove((const char *)arg[0]);
            break;
        case SYS_OPEN:
            get_argument(esp,arg,1);
            check_valid_string((const void *)arg[0], f->esp);
            f->eax = open((const char *)arg[0]);
            unpin_string((void *)arg[0]);
            break;
        case SYS_FILESIZE:
            get_argument(esp,arg,1);
            f->eax = filesize(arg[0]);
            break;
        case SYS_READ:
            get_argument(esp,arg,3);
            check_valid_buffer((void *)arg[1], (unsigned)arg[2], f->esp, true);
            f->eax = read(arg[0],(void *)arg[1],(unsigned)arg[2]);
            unpin_buffer((void *)arg[1], (unsigned) arg[2]);
            break;
        case SYS_WRITE:
            get_argument(esp,arg,3);
            check_valid_buffer((void *)arg[1], (unsigned)arg[2], f->esp, false);
            f->eax = write(arg[0],(void *)arg[1],(unsigned)arg[2]);
            unpin_buffer((void *)arg[1], (unsigned)arg[2]);
            break;
        case SYS_SEEK:
            get_argument(esp,arg,2);
            seek(arg[0],(unsigned)arg[1]);
            break;
        case SYS_TELL:
            get_argument(esp,arg,1);
            f->eax = tell(arg[0]);
            break;
        case SYS_CLOSE:
            get_argument(esp,arg,1);
            close(arg[0]);
            break;
    }
    unpin_ptr(f->esp);
}
/* chack_address function */
    void
check_address(void *addr, void *esp)
{
    struct vm_entry *vme;
    uint32_t address=(unsigned int)addr;
    uint32_t highest_address=0xc0000000;
    uint32_t lowest_address=0x8048000;
    if(address < highest_address && address >= lowest_address){
        vme = find_vme(addr);
        if(vme == NULL){
            if(addr >= esp-STACK_HEURISTIC){
                if(expand_stack(addr) == false)
                    exit(-1);
            }
            else
                exit(-1);
        }
    }
    else
        exit(-1);
}
void check_valid_buffer(void *buffer, unsigned size, void *esp, bool to_write)
{
    struct vm_entry *vme;
    char *check_buffer = (char *)buffer;
    for(unsigned i=0; i<size; i++){
        check_address((void *)check_buffer, esp);
        vme = find_vme((void *)check_buffer);
        if(vme != NULL && to_write == true && vme->writable == false)
            exit(-1);
        check_buffer++;
    }
}
void check_valid_string(const void *str, void *esp)
{
    char *check_str = (char *)str;
    check_address((void *)check_str,esp);
    while(*check_str != 0){
        check_str += 1;
        check_address(check_str, esp);
    }
}
/* get_argument function */
    void
get_argument(void *esp, int *arg, int count)
{
    void *stack_pointer=esp+4;
    if(count <= 0)
        return;
    for(int i=0; i<count; i++){
        check_address(stack_pointer, esp);
        arg[i] = *(int *)stack_pointer;
        stack_pointer = stack_pointer + 4;
    }
}

    void 
halt(void)
{
    shutdown_power_off();
}
    void 
exit(int status)
{
    struct thread *current_process=thread_current();

    current_process->process_exit_status = status;     
    printf("%s: exit(%d)\n",current_process->name,status);
    thread_exit();
}
    int
wait(tid_t tid)
{
    int status;
    status = process_wait(tid);
    return status;
}
    bool
create(const char *file, unsigned initial_size)
{
    bool result=false;
    if(filesys_create(file,initial_size)==true)
        result=true;
    return result;
}
    bool
remove(const char *file)
{
    bool result = false;
    if(filesys_remove(file)==true)
        result = true;
    return result;
}
    tid_t
exec(const char *cmd_name)
{
    tid_t pid;
    struct thread *child_process;

    pid = process_execute(cmd_name);
    child_process = get_child_process(pid);
    sema_down(&(child_process->load_semaphore));      
    if(child_process->load_success==true)
        return pid;
    else
        return -1;
}
    int
open(const char *file)
{
    struct file *new_file;
    int fd;
    new_file=filesys_open(file);

    if(new_file != NULL){
        fd = process_add_file(new_file);
        return fd;
    }
    else
        return -1;
}
    int
filesize(int fd)
{
    int file_size;
    struct file *current_file = process_get_file(fd);
    if(current_file != NULL){
        file_size = file_length(current_file);
        return file_size;
    }
    else
        return -1;
}
    int 
read(int fd, void *buffer, unsigned size)
{
    struct file *current_file;
    int read_size = 0;
    char *read_buffer = (char *)buffer;

    lock_acquire(&file_lock);

    if(fd == 0){
        read_buffer[read_size]=input_getc();
        while(read_size < size && read_buffer[read_size] != '\n'){
            read_size++;
            read_buffer[read_size]=input_getc();
        }
        read_buffer[read_size]='\0';
    }
    else{
        current_file = process_get_file(fd);
        if(current_file != NULL)
            read_size = file_read(current_file,buffer,size);
    }
    lock_release(&file_lock);
    return read_size;
}
    int
write(int fd, void *buffer, unsigned size)
{
    struct file *current_file;
    int write_size = 0;

    if(fd == 1){ 
        putbuf((const char *)buffer,size);
        write_size = size;
    }
    else{
        lock_acquire(&file_lock);
        current_file = process_get_file(fd);
        if(current_file != NULL)
            write_size = file_write(current_file,(const void *)buffer,size);
        lock_release(&file_lock);
    }
    return write_size;
}
    void 
seek(int fd, unsigned position)
{
    struct file *current_file = process_get_file(fd);
    if(current_file != NULL)
        file_seek(current_file,position);
}
    unsigned
tell(int fd)
{
    unsigned offset = 0;
    struct file *current_file = process_get_file(fd);

    if(current_file != NULL)
        offset = file_tell(current_file);
    return offset;
}
    void 
close(int fd)
{
    struct file *current_file = process_get_file(fd);
    if(current_file != NULL){
        file_close(current_file);
        thread_current()->file_descriptor[fd]=NULL;
    }
}

void unpin_ptr(void *vaddr)
{
    struct vm_entry *vme  = find_vme(vaddr);
    if(vme != NULL)
        vme->pinned = false;
}

void unpin_string(void *str)
{
    unpin_ptr(str);
    while(*(char *)str != 0){
        str = (char *)str + 1;
        unpin_ptr(str);
    }
}

void unpin_buffer(void *buffer, unsigned size)
{
    char *local_buffer = (char *)buffer;
    for(int i=0; i<size; i++){
        unpin_ptr(local_buffer);
        local_buffer++;
    }
}
