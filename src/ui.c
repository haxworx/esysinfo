#include "system.h"
#include "process.h"
#include "ui.h"
#include <stdio.h>

typedef struct Ui
{
   Evas_Object *progress_cpu;
   Evas_Object *progress_mem;
} Ui;

typedef struct Results
{
   double cpu_usage;
   long mem_total;
   long mem_used;
   Eina_List *process_list;
} Results;

static long mem_total = 0;
static long mem_used = 0;

void stats_poll(Ecore_Thread *thread)
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
          }

        Results *results = malloc(sizeof(Results));
        results->cpu_usage = cpu_usage;
        results->mem_total = mem_total = memory_total >> 10;
        results->mem_used = mem_used = memory_used >> 10;
        results->process_list = processes;

        ecore_thread_feedback(thread, results);

        EINA_LIST_FREE(list_prev, proc)
          {
             free(proc->command);
             free(proc);
          }

        eina_list_free(list_prev);
        if (ecore_thread_check(thread))
          return;
     }
}

static void
_thread_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Eina_List *l;
   Process_Info *proc;
   Ui *ui = data;
   Results *results = msg;

   EINA_LIST_FREE(results->process_list, proc)
     {
        printf("PID: %d UID: %d CPU: %d CPU%%: %.0f%% COMMAND: %s SIZE: %uK RES: %uK STATE: %s\n",
               proc->pid, proc->uid, proc->cpu_id, proc->cpu_usage, proc->command,
               proc->mem_size >> 10, proc->mem_rss >> 10, proc->state);

        free(proc->command);
        free(proc);
     }

   eina_list_free(results->process_list);

   elm_progressbar_value_set(ui->progress_cpu, (double) results->cpu_usage / 100);
   elm_progressbar_value_set(ui->progress_mem, (double) ((results->mem_total / 100.0) * results->mem_used) / 1000000);

   free(results);
}

static void
_thread_run_cb(void *data, Ecore_Thread *thread)
{
   stats_poll(thread);
}

static void
_thread_end_cb(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   thread = NULL;
}

static void
_thread_error_cb(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   thread = NULL;
}

static char *
_progress_mem_format_cb(double val)
{
  char buf[1024];

  snprintf(buf, sizeof(buf), "%ld M out of %ld M", mem_used, mem_total);

  return strdup(buf);
}

static void
_progress_mem_format_free_cb(char *str)
{
   if (str)
     free(str);
}

Evas_Object *ui_add(Evas_Object *parent)
{
   Evas_Object *box, *hbox, *frame;
   Evas_Object *progress_cpu, *progress_mem;
   Ecore_Thread *thread;
   Ui *ui;

   ui = calloc(1, sizeof(ui));

   thread = ecore_thread_feedback_run(_thread_run_cb, _thread_feedback_cb, _thread_end_cb, _thread_error_cb,
                                      ui, EINA_FALSE);

   box = elm_box_add(parent);
   elm_box_homogeneous_set(box, 1);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);
   elm_object_content_set(parent, box);

   hbox = elm_box_add(box);
   elm_box_homogeneous_set(hbox, 1);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   elm_box_pack_end(box, hbox);
   evas_object_show(hbox);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "System CPU");
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   ui->progress_cpu = progress_cpu = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress_cpu, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress_cpu, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress_cpu, 1.0);
   elm_progressbar_unit_format_set(progress_cpu, "%1.2f%%");
   elm_object_content_set(frame, progress_cpu);
   evas_object_show(progress_cpu);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "System Memory");
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   ui->progress_mem = progress_mem = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress_mem, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress_mem, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress_mem, 1.0);
   elm_progressbar_unit_format_function_set(progress_mem, _progress_mem_format_cb, _progress_mem_format_free_cb);
   elm_object_content_set(frame, progress_mem);
   evas_object_show(progress_mem);

}
