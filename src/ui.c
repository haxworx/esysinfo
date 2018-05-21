#include "system.h"
#include "process.h"
#include "ui.h"
#include <stdio.h>

typedef struct Ui
{
   Evas_Object *progress_cpu;
   Evas_Object *progress_mem;
   Evas_Object *table;
} Ui;

typedef struct Sys_Info
{
   double cpu_usage;
   long mem_total;
   long mem_used;
} Sys_Info;

static long _memory_total = 0;
static long _memory_used = 0;

void stats_poll(Ecore_Thread *thread, Ui *ui)
{
   Eina_List *processes = NULL, *l, *l2;
   Process_Info *proc, *proc_prev;

   while (1)
     {
        processes = proc_info_all_get();
        ecore_thread_feedback(thread, processes);
        if (ecore_thread_check(thread))
          break;
        sleep(10);
     }
}

static void
_thread_system_run(void *data, Ecore_Thread *thread)
{
   int cpu_count;
   double cpu_usage;
   long mem_total, mem_used;

   while (1)
     {
        Sys_Info *results = malloc(sizeof(Sys_Info));

        system_cpu_memory_get(&results->cpu_usage, &results->mem_total, &results->mem_used);

        _memory_total = results->mem_total >>= 10;
        _memory_used = results->mem_used >>= 10;

        ecore_thread_feedback(thread, results);
     }
}

static void
_thread_system_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Ui *ui = data;
   Sys_Info *results = msg;

   elm_progressbar_value_set(ui->progress_cpu, (double) results->cpu_usage / 100);
   elm_progressbar_value_set(ui->progress_mem, (double) ((results->mem_total / 100.0) * results->mem_used) / 1000000);

   free(results);
}

static Evas_Object *
_label_get(Evas_Object *parent, const char *fmt, ...)
{
   char text[1024];
   char buf[1024];
   va_list ap;

   va_start(ap, fmt);
   vsnprintf(text, sizeof(text), fmt, ap);
   va_end(ap);

   snprintf(buf, sizeof(buf), "<hilight>%s</>", text);
   Evas_Object *label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, buf);
   evas_object_show(label);

   return label;
}

static void
_thread_process_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Ui *ui;
   Evas_Object *label;
   Eina_List *procs, *l;
   Process_Info *proc;

   ui = data;
   procs = msg;

   Evas_Object *table = ui->table;

   EINA_LIST_FOREACH(procs, l, proc)
     {
        printf("it is %d and %s\n", proc->pid,proc->command);
     }
}

static void
_thread_process_run(void *data, Ecore_Thread *thread)
{
   Ui *ui = data;

   stats_poll(thread, ui);
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

  snprintf(buf, sizeof(buf), "%ld M out of %ld M", _memory_used, _memory_total);

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
   Evas_Object *box, *hbox, *frame, *table;
   Evas_Object *progress_cpu, *progress_mem, *list;
   Ecore_Thread *thread;
   Ui *ui;

   ui = calloc(1, sizeof(ui));

   ecore_thread_feedback_run(_thread_system_run, _thread_system_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
   ecore_thread_feedback_run(_thread_process_run, _thread_process_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);
   elm_object_content_set(parent, box);

   hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, 0);
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

   ui->table = table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(table);

   Evas_Object *scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_show(scroller);
   elm_object_content_set(scroller, table);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Process Information");
   elm_box_pack_end(box, frame);
   evas_object_show(frame);
   elm_object_content_set(frame, scroller);
}
