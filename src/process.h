#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <Eina.h>

typedef struct Process_Info
{
   pid_t       pid;
   int         uid;
   char       *command;
   int         cpu_id;
   long        cpu_time;
   float       cpu_usage;
   const char *state;
   unsigned long long        mem_size;
   unsigned long long        mem_rss;
} Process_Info;

Eina_List *
process_list_get(void);

#endif
