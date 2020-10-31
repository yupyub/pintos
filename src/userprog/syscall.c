#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
void check_user_vaddr(const void* vaddr);
static void syscall_handler (struct intr_frame *);
void halt(void);
void exit(int status);
tid_t exec(const char* cmd_line);
int wait(tid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
int fibonacci(int n);
int max_of_four_int(int a,int b,int c,int d);


void
syscall_init (void) 
{
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
            break;
        case SYS_REMOVE:
            break;
        case SYS_OPEN:
            break;
        case SYS_FILESIZE:
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
            break;
        case SYS_TELL:
            break;
        case SYS_CLOSE:
            break;
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
    thread_current ()->exit_status = status;
    printf("%s: exit(%d)\n",thread_name(),status);
    thread_exit();
}
tid_t exec(const char* cmd_line){
    return process_execute(cmd_line);
}
int wait(tid_t tid){
    return process_wait(tid);
}
/*
bool create(const char *file, unsigned initial_size){
}
bool remove(const char *file){
}
int filesize(int fd){
}
*/
int read(int fd, void *buffer, unsigned size){
    int i = 0;
    if(fd == STDIN_FILENO){
        for(i = 0;i<(int)size;i++){
            if(((char*)buffer)[i] == '\0')
                break;
        }
    }
    return i;
}
int write(int fd, const void *buffer, unsigned size){
    if(fd == STDOUT_FILENO){
        putbuf(buffer,size);
        return size;
    }
    return -1;
}
/*
void seek(int fd, unsigned position){
}
unsigned tell(int fd){
}
void close(int fd){
}
*/
