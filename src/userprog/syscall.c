#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
static void syscall_handler (struct intr_frame *);
void halt();
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


void
syscall_init (void) 
{
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
/*
static inline void
get_parameter(int32_t *esp,int32_t *Argv,int Argc,void *esp2){
    ASSERT(1<=Argc && Argc<=4);
    while(Argc--){
        check_address_4(++esp,esp2);
        *(args++) = *esp;
    }
}
*/
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    //printf ("system call!\n");
    //hex_dump((uintptr_t)(f->esp),f->esp,100,1);
    //printf("call num : %d\n",*(uint32_t*)(f->esp));
    switch(*(int*)(f->esp)){
        case SYS_HALT:
            //halt();
            break;
        case SYS_EXIT:
            exit(*(int*)(f->esp+4));
            break;
        case SYS_EXEC:
            //exic((const char*)*(uint32_t*)(f->esp+4));
            break;
        case SYS_WAIT:
            wait((tid_t)*(uint32_t*)(f->esp+4));
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
            break;
        case SYS_WRITE:
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
void halt(){
    shutdown_power_off();
}
void exit(int status){
    //thread_current ()->exit_status = status;
    printf("%s: exit(%d)\n",thread_name(),status);
    thread_exit();
}
tid_t exec(const char* cmd_line){
    return process_execute(cmd_line);
}
int wait(tid_t tid){
    return process_wait(tid);
}
bool create(const char *file, unsigned initial_size){
}
bool remove(const char *file){
}
int filesize(int fd){
}
int read(int fd, void *buffer, unsigned size){
    int i;
    if(fd == STDIN_FILENO){
        for(i = 0;i<size;i++){
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
void seek(int fd, unsigned position){
}
unsigned tell(int fd){
}
void close(int fd){
}
