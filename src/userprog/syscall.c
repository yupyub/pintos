#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/off_t.h"
struct file{
    struct inode* inode;
    off_t pos;
    bool deny_write;
};
void check_user_vaddr(const void* vaddr);
static void syscall_handler (struct intr_frame *);
void halt(void);
void exit(int status);
tid_t exec(const char* cmd_line);
int wait(tid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char* file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
int fibonacci(int n);
int max_of_four_int(int a,int b,int c,int d);

struct lock file_lock;
void
syscall_init (void) 
{
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&file_lock);
}
void check_user_vaddr(const void* vaddr){
    if(!is_user_vaddr(vaddr))
        exit(-1);
}
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    //printf ("system call!\n");
    //hex_dump((uintptr_t)(f->esp),f->esp,100,1);
    //printf("call num : %d\n",*(uint32_t*)(f->esp));
    check_user_vaddr(f->esp);
    switch(*(int*)(f->esp)){
        case SYS_FIBONACCI:
            check_user_vaddr(f->esp+4);
            f->eax=fibonacci(*(int*)(f->esp+4));
            break;
        case SYS_MAX_OF_FOUR_INTEGERS:
            check_user_vaddr(f->esp+4);
            check_user_vaddr(f->esp+8);
            check_user_vaddr(f->esp+12);
            check_user_vaddr(f->esp+16);
            f->eax=max_of_four_int(*(int*)(f->esp+4),*(int*)(f->esp+8),*(int*)(f->esp+12),*(int*)(f->esp+16));
            break;
        case SYS_HALT:
            halt();
            break;
        case SYS_EXIT:
            check_user_vaddr(f->esp+4);
            exit(*(int*)(f->esp+4));
            break;
        case SYS_EXEC:
            check_user_vaddr(f->esp+4);
            f->eax = exec((const char*)*(uint32_t*)(f->esp+4));
            break;
        case SYS_WAIT:
            check_user_vaddr(f->esp+4);
            f->eax = wait((tid_t)*(uint32_t*)(f->esp+4));
            break;
        case SYS_CREATE:
            check_user_vaddr(f->esp+4);
            check_user_vaddr(f->esp+8);
            f->eax = create((const char*)*(uint32_t*)(f->esp+4),(unsigned)*(uint32_t*)(f->esp+8));
            break;
        case SYS_REMOVE:
            check_user_vaddr(f->esp+4);
            f->eax = remove((const char*)*(uint32_t*)(f->esp+4));
            break;
        case SYS_OPEN:
            check_user_vaddr(f->esp+4);
            f->eax = open((const char*)*(uint32_t*)(f->esp+4));
            break;
        case SYS_FILESIZE:
            check_user_vaddr(f->esp+4);
            f->eax = filesize((int)*(uint32_t*)(f->esp+4));
            break;
        case SYS_READ:
            check_user_vaddr(f->esp+4);
            check_user_vaddr(f->esp+8);
            check_user_vaddr(f->esp+12);
            f->eax = read((int)*(uint32_t*)(f->esp+4),(void*)*(uint32_t*)(f->esp+8),(unsigned)*(uint32_t*)(f->esp+12));
            break;
        case SYS_WRITE:
            check_user_vaddr(f->esp+4);
            check_user_vaddr(f->esp+8);
            check_user_vaddr(f->esp+12);
            f->eax = write((int)*(uint32_t*)(f->esp+4),(void*)*(uint32_t*)(f->esp+8),(unsigned)*(uint32_t*)(f->esp+12));
            break;
        case SYS_SEEK:
            check_user_vaddr(f->esp+4);
            check_user_vaddr(f->esp+8);
            seek((int)*(uint32_t*)(f->esp+4),(unsigned)*(uint32_t*)(f->esp+8));
            break;
        case SYS_TELL:
            check_user_vaddr(f->esp+4);
            f->eax = tell((int)*(uint32_t*)(f->esp+4));
            break;
        case SYS_CLOSE:
            check_user_vaddr(f->esp+4);
            close((int)*(uint32_t*)(f->esp+4));
            break;
        default:
            exit(-1); // syscall number is invalid
    }
    //thread_exit ();
}
int fibonacci(int n){
    int f0 = 0, f1 = 1, f2;
    for(int i=0;i<n;i++){
        f2=f0+f1;
        f0=f1;
        f1=f2;
   }
   return f0;
}
int max_of_four_int(int a,int b,int c,int d){
    int maxi = a;
    if(maxi < b)
        maxi = b;
    if(maxi < c)
        maxi = c;
    if(maxi < d)
        maxi = d;
    return maxi;
}
void halt(void){
    shutdown_power_off();
}
void exit(int status){
    thread_current()->exit_status = status;
    printf("%s: exit(%d)\n",thread_name(),status);
    ////
    for(int i = 3;i<128;i++){
        if(thread_current()->fd[i] != NULL)
            close(i);
    }
    ////
    thread_exit();
}
tid_t exec(const char* cmd_line){
    return process_execute(cmd_line);
}
int wait(tid_t tid){
    return process_wait(tid);
}
int read(int fd, void *buffer, unsigned size){
    int i = 0;
    check_user_vaddr(buffer); //// ????
    lock_acquire(&file_lock);
    if(fd == STDIN_FILENO){
        for(i = 0;i<(int)size;i++){
            if(((char*)buffer)[i] == '\0')
                break;
        }
    }
    else if(fd>2){
        if(thread_current()->fd[fd] == NULL){
            lock_release(&file_lock);
            exit(-1);
            //return -1;
        }
        i = file_read(thread_current()->fd[fd],buffer,size);
    }
    lock_release(&file_lock);
    return i;
}
int write(int fd, const void *buffer, unsigned size){
    int ret = -1;
    check_user_vaddr(buffer);
    lock_acquire(&file_lock);

    if(fd == STDOUT_FILENO){
        putbuf(buffer,size);
        ret = size;
    }
    else if(fd>2){
        if(thread_current()->fd[fd] == NULL){
            lock_release(&file_lock);
            exit(-1);
            //return 0;
        }
        if((thread_current()->fd[fd])->deny_write)
            file_deny_write(thread_current()->fd[fd]);
        ret = file_write(thread_current()->fd[fd],buffer,size);
    }
    lock_release(&file_lock);
    return ret;
}

bool create(const char *file, unsigned initial_size){
    if(file == NULL)
        exit(-1);
    //check_user_vaddr(file);
    return filesys_create(file,initial_size);
}
bool remove(const char *file){
    if(file == NULL)
        exit(-1);
    //check_user_vaddr(file);
    return filesys_remove(file);
}
int open(const char* file){
    if(file == NULL)
        exit(-1);
    int ret = -1;
    check_user_vaddr(file);
    lock_acquire(&file_lock);
    struct file* fp = filesys_open(file);
    if(fp == NULL){
        lock_release(&file_lock);
        return ret;
    }
    for(int i = 3;i<128;i++){
        if(thread_current()->fd[i] == NULL){
            if(strcmp(thread_current()->name,file) == 0)
                file_deny_write(fp);
            thread_current()->fd[i] = fp;
            //return i;
            ret = i;
            break;
        }
    }
    lock_release(&file_lock);
    return ret;
}
int filesize(int fd){
    if(thread_current()->fd[fd] == NULL)
        exit(-1);
    return file_length(thread_current()->fd[fd]);
}
void seek(int fd, unsigned position){
    if(thread_current()->fd[fd] == NULL)
        exit(-1);
    file_seek(thread_current()->fd[fd],position);
}
unsigned tell(int fd){
    if(thread_current()->fd[fd] == NULL)
        exit(-1);
    return file_tell(thread_current()->fd[fd]);
}
void close(int fd){
    if(thread_current()->fd[fd] == NULL)
        exit(-1);
    struct file* fp = thread_current()->fd[fd];
    thread_current()->fd[fd] = NULL;
    file_close(fp);
}

