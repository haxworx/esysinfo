#include "system.h"
#include "process.h"
#include <stdio.h>

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
        printf("it is %s\n", proc->command);
     }

   printf("nCPU: %d CPU: %.02f%% Mem %ld and %ld\n", cpu_count, cpu_usage, memory_total, memory_used);

   EINA_LIST_FREE(processes, proc)
     {
        free(proc->command);
        free(proc->login);
     }

   eina_list_free(processes);

   eina_shutdown();
}
