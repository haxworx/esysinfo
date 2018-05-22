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
   int8_t      nice;
   int8_t      priority;
   int         cpu_id;
   int32_t     numthreads;
   int64_t     mem_size;
   int64_t     mem_rss;
   char        command[CMD_NAME_MAX];
   const char *state;

   // Not used yet in UI.
   long        cpu_time;
   double      cpu_usage;
} Process_Info;

typedef enum {
   PROCESS_INFO_FIELD_PID,
   PROCESS_INFO_FIELD_UID,
   PROCESS_INFO_FIELD_NICE,
   PROCESS_INFO_FIELD_PRI,
   PROCESS_INFO_FIELD_CPU,
   PROCESS_INFO_FIELD_THREADS,
   PROCESS_INFO_FIELD_SIZE,
   PROCESS_INFO_FIELD_RSS,
   PROCESS_INFO_FIELD_COMMAND,
   PROCESS_INFO_FIELD_STATE,

   // Not used yet in UI.
   PROCESS_INFO_FIELD_CPU_TIME,
   PROCESS_INFO_FIELD_CPU_USE,
} Process_Info_Field;

#define PROCESS_INFO_FIELDS 10

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
