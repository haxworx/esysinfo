#include "system.h"
#include "process.h"
#include <stdio.h>
#include <pwd.h>

int main(void)
{
   Eina_List *processes, *l;
   Process_Info *proc;
   double cpu_usage;
   int cpu_count;
   long memory_total, memory_used;

   eina_init();

   cpu_count = system_cpu_memory_get(&cpu_usage, &memory_total, &memory_used);

   processes = process_list_get();
   EINA_LIST_FOREACH(processes, l, proc)
     {
        printf("PID: %d ", proc->pid);
        struct passwd *pwent = getpwuid(proc->uid);
        if (pwent)
          printf("USER: %s ", pwent->pw_name);
        printf("UID: %d CPU: %d COMMAND: %s SIZE: %ldK RES: %ldK STATE: %s\n", proc->uid, proc->cpu_id, proc->command,
               proc->mem_size >> 10, proc->mem_rss >> 10, proc->state);
     }

   printf("nCPU: %d CPU: %.02f%% Mem %ld/%ldM\n", cpu_count, cpu_usage, memory_used >> 10, memory_total >> 10);

   EINA_LIST_FREE(processes, proc)
     {
        free(proc->command);
     }

   eina_list_free(processes);

   eina_shutdown();
}
