#ifndef __PROC_H__
#define __PROC_H__

/**
 * @file
 * @brief Routines for querying processes.
 */

/**
 * @brief Querying Processes
 * @defgroup Proc
 *
 * @{
 *
 * Query processes.
 *
 */

#include <Eina.h>
#include <stdint.h>
#include <unistd.h>

#define CMD_NAME_MAX 256

typedef struct _Process_Info 
{
   pid_t       pid;
   uid_t       uid;
   char        command[CMD_NAME_MAX];
   int         cpu_id;
   long        cpu_time;
   int8_t      priority;
   int8_t      nice;
   int32_t     numthreads;
   const char *state;
   int64_t    mem_size;
   int64_t    mem_rss;
} Process_Info;

/**
 * Query a full list of running processes and return a list.
 *
 * @return A list of proc_t members for all processes.
 */
Eina_List *
proc_info_all_get(void);

/**
 * Query a process for its current state.
 *
 * @param pid The process ID to query.
 *
 * @return A proc_t pointer containing the process information.
 */
Process_Info  *
proc_info_by_pid(int pid);

/**
 * @}
 */

#endif
