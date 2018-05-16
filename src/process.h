#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <Eina.h>

typedef enum
{
   PROCESS_STATE_SLEEPING = 0,
   PROCESS_STATE_ACTIVE,
} state_t;

typedef struct Process_Info
{
   pid_t   pid;
   int     uid;
   char   *command;
   int     cpu_id;
   long    cpu_time;
   state_t state;
   long    mem_size;
   long    mem_rss;
   char   *login;
} Process_Info;

Eina_List *
process_list_get(void);

#endif
