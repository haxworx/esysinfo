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
#include <Ecore.h>
#include <Ecore_File.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "process.h"

#if !defined(PID_MAX)
# define PID_MAX 99999
#endif

static Eina_List *_process_list_linux_get(void);
static Eina_List *_process_list_freebsd_get(void);
static Eina_List *_process_list_macos_get(void);

static const char *
_process_state_name(char state)
{
   const char *statename = NULL;
#if defined(__linux__)

   switch (state)
     {
        case 'D':
          statename = "USLEEP";
        break;

        case 'I':
          statename = "IDLE";
        break;

        case 'R':
          statename = "RUN";
        break;

        case 'S':
          statename = "SLEEP";
        break;

        case 'T':
        case 't':
          statename = "STOP";
        break;

        case 'Z':
          statename = "ZOMB";
        break;
     }
#else
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
#endif
   return statename;
}

Eina_List *
process_list_get(void)
{
   Eina_List *processes;

#if defined(__linux__)
   processes = _process_list_linux_get();
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   processes = _process_list_freebsd_get();
#elif defined(__MacOS__)
   processes = _process_list_macos_get();
#else
   processes = NULL;
#endif

   return processes;
}

#if defined(__linux__)

static unsigned long
_parse_line(const char *line)
{
   char *p, *tok;

   p = strchr(line, ':') + 1;
   while (isspace(*p))
     p++;
   tok = strtok(p, " ");

   return atol(tok);
}

static Eina_List *
_process_list_linux_get(void)
{
   char *name;
   Eina_List *files, *l, *list = NULL;
   FILE *f;
   char path[PATH_MAX], line[4096];
   int pid;

   int pagesize = getpagesize();

   files = ecore_file_ls("/proc");
   EINA_LIST_FOREACH(files, l, name)
     {
        char state, program_name[1024];
        int res, utime, stime, cutime, cstime;
        int uid, psr, mem_size, mem_rss, mem_shared;

        pid = atoi(name);
        if (!pid) continue;

        snprintf(path, sizeof(path), "/proc/%d/stat", pid);

        f = fopen(path, "r");
        if (!f) continue;

        if (fgets(line, sizeof(line), f))
          {
             int dummy;
             char *end, *start = strchr(line, '(') + 1;
             end = strchr(line, ')');
             strncpy(program_name, start, end - start);
             program_name[end - start] = '\0';

             res = sscanf (end + 2, "%c %d %d %d %d %d %u %u %u %u %u %d %d %d %d %d %d %u %u %d %u %u %u %u %u %u %u %u %d %d %d %d %u %d %d %d %d %d %d %d %d %d",
                           &state, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &utime, &stime, &cutime, &cstime,
                           &dummy, &dummy, &dummy, &dummy, &dummy, &mem_size, &mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                           &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &psr, &dummy, &dummy, &dummy, &dummy, &dummy);
          }
        fclose(f);

        if (res != 42) continue;

        snprintf(path, sizeof(path), "/proc/%d/status", pid);

        f = fopen(path, "r");
        if (!f) continue;

        while ((fgets(line, sizeof(line), f)) != NULL)
          {
             if (!strncmp(line, "Uid:", 4))
               {
                   uid = _parse_line(line);
                   break;
               }
          }
        fclose(f);

        Process_Info *p = calloc(1, sizeof(Process_Info));

        p->pid = pid;
        p->uid = uid;
        p->cpu_id = psr;
        p->command = strdup(program_name);
        p->state = _process_state_name(state);
        p->cpu_time = utime + stime;
        p->mem_size = mem_size;
        p->mem_rss = mem_rss * pagesize;

        list = eina_list_append(list, p);
     }

   EINA_LIST_FREE(files, name)
     {
        free(name);
     }

   if (files)
     eina_list_free(files);

   return list;
}
#endif

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
   int pagesize = getpagesize();

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
        p->mem_rss = kp.ki_rssize * pagesize;

        list = eina_list_append(list, p);
     }

   return list;
}
#endif

