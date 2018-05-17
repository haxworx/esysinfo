#include "system.h"
#include "process.h"
#include <stdio.h>

void stats_poll(void)
{
   Eina_List *list_prev, *processes, *l, *l2;
   Process_Info *proc, *proc_prev;
   double cpu_usage;
   int cpu_count;
   long memory_total, memory_used;

   while (1)
     {
        list_prev = process_list_get();

        cpu_count = system_cpu_memory_get(&cpu_usage, &memory_total, &memory_used);

        processes = process_list_get();
        EINA_LIST_FOREACH(processes, l, proc)
          {
             double proc_cpu_usage = 0.0;
             EINA_LIST_FOREACH(list_prev, l2, proc_prev)
               {
                  if (proc->pid == proc_prev->pid)
                    {
                       double diff = proc->cpu_time - proc_prev->cpu_time;
                       if (diff)
                         {
                            proc_cpu_usage = (diff / 100.0) * 100.0;
                         }
                       break;
                    }
               }
             proc->cpu_usage = proc_cpu_usage;
             printf("PID: %d UID: %d CPU: %d CPU%%: %.0f%% COMMAND: %s SIZE: %uK RES: %uK STATE: %s\n",
                    proc->pid, proc->uid, proc->cpu_id, proc->cpu_usage, proc->command,
                    proc->mem_size >> 10, proc->mem_rss >> 10, proc->state);
          }

        printf("nCPU: %d CPU: %.02f%% Mem %ld/%ldM\n", cpu_count, cpu_usage, memory_used >> 10, memory_total >> 10);

        EINA_LIST_FREE(processes, proc)
          {
             free(proc->command);
             free(proc);
          }

        EINA_LIST_FREE(list_prev, proc)
          {
             free(proc->command);
             free(proc);
          }

        eina_list_free(processes);
        eina_list_free(list_prev);
     }
}
