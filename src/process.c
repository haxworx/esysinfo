#if defined(__FreeBSD__) || defined(__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/user.h>
# include <sys/proc.h>
#endif

#include <Eina.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "process.h"

#if !defined(PID_MAX)
# define PID_MAX 99999
#endif

static Eina_List *_process_list_freebsd_get(void);

Eina_List *
process_list_get(void)
{
   Eina_List *processes;

#if defined(__FreeBSD__) || defined(__DragonFly__)
   processes = _process_list_freebsd_get();
#else
   processes = NULL;
#endif

   return processes;
}

#if defined(__FreeBSD__) || defined(__DragonFly__)
static Eina_List *
_process_list_freebsd_get(void)
{
   Eina_List *list = NULL;
   struct kinfo_proc kp;
   int mib[6];
   size_t len;
   int page_size = getpagesize();

   len = sizeof(int);
   if (sysctlnametomib("kern.proc.pid", mib, &len) == -1)
     return NULL;

   for (int i = 1; i <= PID_MAX; i++)
     {
        mib[3] = i;
        len = sizeof(kp);
        if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
          {
             continue;
          }

        Process_Info *p = malloc(sizeof(Process_Info));

        p->pid = kp.ki_pid;
        p->command = strdup(kp.ki_comm);
        p->cpu_id = kp.ki_oncpu;
        if (p->cpu_id == -1)
          p->cpu_id = kp.ki_lastcpu;

        struct rusage usage = kp.ki_rusage;

        p->cpu_time = (usage.ru_utime.tv_sec * 1000000) + usage.ru_utime.tv_usec + (usage.ru_stime.tv_sec * 1000000) + usage.ru_stime.tv_usec; 
        p->state = kp.ki_stat;
        p->mem_size = kp.ki_size;
        p->mem_rss = kp.ki_rssize * page_size;
        p->login = strdup(kp.ki_login);

        list = eina_list_append(list, p);
     }

   return list;
}
#endif

