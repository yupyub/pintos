#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#define MAX_STACK_SIZE (1 << 23)

#include "vm/page.h"
#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool setup_stack (void **esp);
struct file *process_get_file(int fd);
void process_close_file(int fd);
struct thread *get_child_process(int pid);
void remove_child_process(struct thread *cp);
void argument_stack(char **parse, int count, void **esp);
int process_add_file(struct file *f);
void process_exit(void);
bool expand_stack(void *addr);
bool handle_mm_fault(struct vm_entry *vme);
#endif /* userprog/process.h */
