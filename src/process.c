#if defined(__MACH__) && defined(__APPLE__)
# define __MacOS__
#endif

#if defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/user.h>
# include <sys/proc.h>
#endif

#if defined(__OpenBSD__)
# include <kvm.h>
# include <limits.h>
# include <sys/proc.h>
# include <sys/param.h>
#endif

#if defined(__MacOS__)
# include <libproc.h>
# include <sys/proc_info.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#include "process.h"
#include <Eina.h>
#include <Ecore.h>
#include <Ecore_File.h>

#if !defined(PID_MAX)
# define PID_MAX 99999
#endif

static const char *
_process_state_name(char state)
{
   const char *statename = NULL;
#if defined(__linux__)

   switch (state)
     {
        case 'D':
          statename = "DSLEEP";
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

        case 'X':
          statename = "DEAD";
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
#if !defined(__OpenBSD__)
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
#endif
#if defined(__OpenBSD__)
        case SDEAD:
          statename = "DEAD";
        break;

        case SONPROC:
          statename = "ONPROC";
        break;
#endif
     }
#endif
   return statename;
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

   char path[PATH_MAX], line[4096], program_name[1024], state;
   int pid, res, utime, stime, cutime, cstime, uid, psr, pri, nice, numthreads;
   unsigned int mem_size, mem_rss;

   int pagesize = getpagesize();

   files = ecore_file_ls("/proc");
   EINA_LIST_FOREACH(files, l, name)
     {
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
                           &pri, &nice, &numthreads, &dummy, &dummy, &mem_size, &mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
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
        snprintf(p->command, sizeof(p->command), "%s", program_name);
        p->state = _process_state_name(state);
        p->cpu_time = utime + stime;
        p->mem_size = mem_size;
        p->mem_rss = mem_rss * pagesize;
        p->nice = nice;
        p->priority = pri;
        p->numthreads = numthreads;

        list = eina_list_append(list, p);
     }

   EINA_LIST_FREE(files, name);
     {
        free(name);
     }

   if (files)
     eina_list_free(files);

   return list;
}

Process_Info *
proc_info_by_pid(int pid)
{
   FILE *f;
   char path[PATH_MAX];
   char line[4096];
   char state, program_name[1024];
   int res, dummy, utime, stime, cutime, cstime, uid, psr;
   unsigned int mem_size, mem_rss, pri, nice, numthreads;

   snprintf(path, sizeof(path), "/proc/%d/stat", pid);
   if (!ecore_file_exists(path))
     return NULL;

   f = fopen(path, "r");
   if (!f) return NULL;

   if (fgets(line, sizeof(line), f))
     {
        char *end, *start = strchr(line, '(') + 1;
        end = strchr(line, ')');
        strncpy(program_name, start, end - start);
        program_name[end - start] = '\0';

        res = sscanf (end + 2, "%c %d %d %d %d %d %u %u %u %u %u %d %d %d %d %d %d %u %u %d %u %u %u %u %u %u %u %u %d %d %d %d %u %d %d %d %d %d %d %d %d %d",
                      &state, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &utime, &stime, &cutime, &cstime,
                      &pri, &nice, &numthreads, &dummy, &dummy, &mem_size, &mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                      &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &psr, &dummy, &dummy, &dummy, &dummy, &dummy);
     }
   fclose(f);

   if (res != 42) return NULL;

   snprintf(path, sizeof(path), "/proc/%d/status", pid);

   f = fopen(path, "r");
   if (!f) return NULL;

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
   snprintf(p->command, sizeof(p->command), "%s", program_name);
   p->state = _process_state_name(state);
   p->cpu_time = utime + stime;
   p->mem_size = mem_size;
   p->mem_rss = mem_rss * getpagesize();
   p->priority = pri;
   p->nice = nice;
   p->numthreads = numthreads;

   return p;
}

#endif

#if defined(__OpenBSD__)

Process_Info *
proc_info_by_pid(int pid)
{
   struct kinfo_proc *kp;
   kvm_t *kern;
   char errbuf[4096];
   int count, pagesize, pid_count;

   kern = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (!kern) return NULL;

   kp = kvm_getprocs(kern, KERN_PROC_PID, pid, sizeof(*kp), &count);
   if (!kp) return NULL;

   if (count == 0) return NULL;
   pagesize = getpagesize();

   Process_Info *p = malloc(sizeof(Process_Info));
   p->pid = kp->p_pid;
   p->uid = kp->p_uid;
   p->cpu_id = kp->p_cpuid;
   snprintf(p->command, sizeof(p->command), "%s", kp->p_comm);
   p->state = _process_state_name(kp->p_stat);
   p->cpu_time = kp->p_cpticks;
   p->mem_size = (kp->p_vm_tsize * pagesize) + (kp->p_vm_dsize * pagesize) + (kp->p_vm_ssize * pagesize);
   p->mem_rss = kp->p_vm_rssize * pagesize;
   p->priority = kp->p_priority - PZERO;
   p->nice = kp->p_nice - NZERO;
   p->numthreads = -1;

   kp = kvm_getprocs(kern, KERN_PROC_SHOW_THREADS, 0, sizeof(*kp), &pid_count);

   for (int i = 0; i < pid_count; i++)
     {
        if (kp[i].p_pid == p->pid)
          p->numthreads++;
     }

   return p;
}

static Eina_List *
_process_list_openbsd_get(void)
{
   struct kinfo_proc *kp;
   Process_Info *p;
   char errbuf[4096];
   kvm_t *kern;
   int pid_count, pagesize;
   Eina_List *l, *list = list_new();

   kern = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (!kern) return NULL;

   kp = kvm_getprocs(kern, KERN_PROC_ALL, 0, sizeof(*kp), &pid_count);
   if (!kp) return NULL;

   pagesize = getpagesize();

   for (int i = 0; i < pid_count; i++)
     {
        p = malloc(sizeof(Process_Info));
        p->pid = kp[i].p_pid;
        p->uid = kp[i].p_uid;
        p->cpu_id = kp[i].p_cpuid;
        snprintf(p->command, sizeof(p->command), "%s", kp[i].p_comm);
        p->state = _process_state_name(kp[i].p_stat);
        p->cpu_time = kp[i].p_cpticks;
        p->mem_size = (kp[i].p_vm_tsize * pagesize) + (kp[i].p_vm_dsize * pagesize) + (kp[i].p_vm_ssize * pagesize);
        p->mem_rss = kp[i].p_vm_rssize * pagesize;
	p->priority = kp[i].p_priority - PZERO;
	p->nice = kp[i].p_nice - NZERO;
        p->numthreads = -1;
        list = eina_list_append(list, p);
     }

   kp = kvm_getprocs(kern, KERN_PROC_SHOW_THREADS, 0, sizeof(*kp), &pid_count);

   EINA_LIST_FOREACH(list, l, p)
     {
        for (int i = 0; i < pid_count; i++)
          {
             if (kp[i].p_pid == p->pid)
	       p->numthreads++;
          }
     }

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

        struct proc_taskinfo taskinfo;
        int size = proc_pidinfo(i, PROC_PIDTASKINFO, 0, &taskinfo, sizeof(taskinfo));
        if (size != sizeof(taskinfo)) continue;

        struct proc_workqueueinfo workqueue;

        size = proc_pidinfo(i, PROC_PIDWORKQUEUEINFO, 0, &workqueue, sizeof(workqueue));
        Process_Info *p = calloc(1, sizeof(Process_Info));

        p->pid = kp.kp_proc.p_pid;
        p->uid = kp.kp_eproc.e_ucred.cr_uid;
        /* Actually WQ# */
        p->cpu_id = workqueue.pwq_nthreads;

        snprintf(p->command, sizeof(p->command), "%s", kp.kp_proc.p_comm);
        p->cpu_time = taskinfo.pti_total_user + taskinfo.pti_total_system;
        p->state = _process_state_name(kp.kp_proc.p_stat);
        p->mem_size = taskinfo.pti_virtual_size;
        p->mem_rss = taskinfo.pti_resident_size;
        p->priority = kp.kp_proc.p_priority;
        p->nice = kp.kp_proc.p_nice;
        p->numthreads = taskinfo.pti_threadnum;

        list = eina_list_append(list, p);
     }

   return list;
}

Process_Info *
proc_info_by_pid(int pid)
{
   struct kinfo_proc kp;
   struct proc_taskinfo taskinfo;
   struct proc_workqueueinfo workqueue;
   size_t len;
   int size, mib[6];

   len = sizeof(int);
   if (sysctlnametomib("kern.proc.pid", mib, &len) == -1)
     return NULL;

   mib[4] = pid;

   len = sizeof(kp);
   if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
     return NULL;

   size = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &taskinfo, sizeof(taskinfo));
   if (size != sizeof(taskinfo))
     return NULL;

   size = proc_pidinfo(pid, PROC_PIDWORKQUEUEINFO, 0, &workqueue, sizeof(workqueue));
   if (size != sizeof(workqueue))
     return NULL;

   Process_Info *p = calloc(1, sizeof(Process_Info));
   p->pid = kp.kp_proc.p_pid;
   p->uid = kp.kp_eproc.e_ucred.cr_uid;
   p->cpu_id = workqueue.pwq_nthreads;
   snprintf(p->command, sizeof(p->command), "%s", kp.kp_proc.p_comm);
   p->cpu_time = taskinfo.pti_total_user + taskinfo.pti_total_system;
   p->state = _process_state_name(kp.kp_proc.p_stat);
   p->mem_size = taskinfo.pti_virtual_size;
   p->mem_rss = taskinfo.pti_resident_size;
   p->priority = kp.kp_proc.p_priority;
   p->nice = kp.kp_proc.p_nice;
   p->numthreads = taskinfo.pti_threadnum;

   return p;
}

#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
static Eina_List *
_process_list_freebsd_get(void)
{
   Eina_List *list;
   struct rusage *usage;
   struct kinfo_proc kp;
   int mib[6];
   size_t len;
   int pagesize = getpagesize();

   list = NULL;

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
        snprintf(p->command, sizeof(p->command), "%s", kp.ki_comm);
        p->cpu_id = kp.ki_oncpu;
        if (p->cpu_id == -1)
          p->cpu_id = kp.ki_lastcpu;

        usage = &kp.ki_rusage;

        p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
        p->state = _process_state_name(kp.ki_stat);
        p->mem_size = kp.ki_size;
        p->mem_rss = kp.ki_rssize * pagesize;
        p->nice =  kp.ki_nice - NZERO;
        p->priority = kp.ki_pri.pri_level - PZERO;
        p->numthreads = kp.ki_numthreads;

        list = eina_list_append(list, p);
     }

   return list;
}

Process_Info *
proc_info_by_pid(int pid)
{
   struct rusage *usage;
   struct kinfo_proc kp;
   int mib[6];
   size_t len;
   int pagesize = getpagesize();

   len = sizeof(int);
   if (sysctlnametomib("kern.proc.pid", mib, &len) == -1)
     return NULL;

   mib[3] = pid;

   len = sizeof(kp);
   if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
     return NULL;

   Process_Info *p = calloc(1, sizeof(Process_Info));
   p->pid = kp.ki_pid;
   p->uid = kp.ki_uid;
   snprintf(p->command, sizeof(p->command), "%s", kp.ki_comm);
   p->cpu_id = kp.ki_oncpu;
   if (p->cpu_id == -1)
     p->cpu_id = kp.ki_lastcpu;

   usage = &kp.ki_rusage;

   p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
   p->state = _process_state_name(kp.ki_stat);
   p->mem_size = kp.ki_size;
   p->mem_rss = kp.ki_rssize * pagesize;
   p->nice = kp.ki_nice = NZERO;
   p->priority = kp.ki_pri.pri_level - PZERO;
   p->numthreads = kp.ki_numthreads;

   return p;
}

#endif

Eina_List *
proc_info_all_get(void)
{
   Eina_List *processes;

#if defined(__linux__)
   processes = _process_list_linux_get();
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   processes = _process_list_freebsd_get();
#elif defined(__MacOS__)
   processes = _process_list_macos_get();
#elif defined(__OpenBSD__)
   processes = _process_list_openbsd_get();
#else
   processes = NULL;
#endif

   return processes;
}

