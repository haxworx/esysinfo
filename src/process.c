#if defined(__MACH__) && defined(__APPLE__)
# define __MacOS__
#endif

#if defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/user.h>
# include <sys/proc.h>
#endif

#if defined(__MacOS__)
# include <libproc.h>
# include <sys/proc_info.h>
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
static Eina_List *_process_list_macos_get(void);

static const char *
_process_state_name(char state)
{
   const char *statename = NULL;

   switch (state)
     {
        case SIDL:
           statename = "IDLE";
        break;

        case SRUN:
           statename = "RUN";
        break;

        case SSLEEP:
          statename = "SLEEP";
        break;

        case SSTOP:
          statename = "STOP";
        break;
#if !defined(__MacOS__)
        case SWAIT:
          statename = "WAIT";
        break;

        case SLOCK:
           statename = "LOCK";
        break;
#endif
        case SZOMB:
          statename = "ZOMB";
        break;
     }

   return statename;
}

Eina_List *
process_list_get(void)
{
   Eina_List *processes;

#if defined(__FreeBSD__) || defined(__DragonFly__)
   processes = _process_list_freebsd_get();
#elif defined(__MacOS__)
   processes = _process_list_macos_get();
#else
   processes = NULL;
#endif

   return processes;
}

#if defined(__MacOS__)
static Eina_List *
_process_list_macos_get(void)
{
   Eina_List *list = NULL;
   struct kinfo_proc kp;
   int mib[6];
   size_t len;

   len = sizeof(int);
   if (sysctlnametomib("kern.proc.pid", mib, &len) == -1)
     return NULL;

   for (int i = 1; i <= PID_MAX; i++)
     {
        mib[3] = i;
        len = sizeof(kp);
        if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
          continue;

        struct proc_taskinfo task;
        int size = proc_pidinfo(i, PROC_PIDTASKINFO, 0, &task, sizeof(task));
        if (size != sizeof(task)) continue;

        Process_Info *p = calloc(1, sizeof(Process_Info));

        p->pid = kp.kp_proc.p_pid;
        p->uid = kp.kp_eproc.e_ucred.cr_uid;
        p->cpu_id = -1;

        p->command = strdup(kp.kp_proc.p_comm);
        p->cpu_time = task.pti_total_user + task.pti_total_system;
        p->state = _process_state_name(kp.kp_proc.p_stat);
        p->mem_size = task.pti_virtual_size;
        p->mem_rss = task.pti_resident_size;

        list = eina_list_append(list, p);
     }

   return list;
}

#endif

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

        Process_Info *p = calloc(1, sizeof(Process_Info));

        p->pid = kp.ki_pid;
        p->uid = kp.ki_uid;
        p->command = strdup(kp.ki_comm);
        p->cpu_id = kp.ki_oncpu;
        if (p->cpu_id == -1)
          p->cpu_id = kp.ki_lastcpu;

        struct rusage usage = kp.ki_rusage;

        p->cpu_time = (usage.ru_utime.tv_sec * 1000000) + usage.ru_utime.tv_usec + (usage.ru_stime.tv_sec * 1000000) + usage.ru_stime.tv_usec; 
        p->state = _process_state_name(kp.ki_stat);
        p->mem_size = kp.ki_size;
        p->mem_rss = kp.ki_rssize * page_size;

        list = eina_list_append(list, p);
     }

   return list;
}
#endif

