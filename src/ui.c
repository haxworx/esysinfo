#include "system.h"
#include "process.h"
#include "ui.h"
#include <stdio.h>

typedef struct Ui
{
   Evas_Object *progress_cpu;
   Evas_Object *progress_mem;
   Evas_Object *list;
} Ui;

typedef struct Sys_Info
{
   double cpu_usage;
   long mem_total;
   long mem_used;
} Sys_Info;

static long _memory_total = 0;
static long _memory_used = 0;

static void
_list_content_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Process_Info *proc = data;

   free(proc->command);
   free(proc);
}

static Evas_Object *
_label_get(Evas_Object *parent, const char *fmt, ...)
{
   char text[1024];
   va_list ap;

   va_start(ap, fmt);
   vsnprintf(text, sizeof(text), fmt, ap);
   va_end(ap);

   Evas_Object *label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, text);
   evas_object_show(label);

   return label;
}

static Evas_Object *
_list_content_get(void *data, Evas_Object *obj, const char *source)
{
   Process_Info *proc = data;
   Evas_Object *box, *label;
   Evas_Object *table;
   char buf[1024];
   if (strcmp(source, "elm.swallow.content"))
     return NULL;

   table = elm_table_add(obj);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(table);

   label = _label_get(obj, "%d", proc->pid);
   elm_table_pack(table, label, 0, 0, 1, 1);

   label = _label_get(obj, "%d", proc->uid);
   elm_table_pack(table, label, 1, 0, 1, 1);

   label = _label_get(obj, "%d%%", proc->cpu_usage);
   elm_table_pack(table, label, 2, 0, 1, 1);

   label = _label_get(obj, "%d", proc->cpu_id);
   elm_table_pack(table, label, 3, 0, 1, 1);

   label = _label_get(obj, "%ldK", proc->mem_size);
   elm_table_pack(table, label, 4, 0, 1, 1);

   label = _label_get(obj, "%ldK", proc->mem_rss);
   elm_table_pack(table, label, 5, 0, 1, 1);

   label = _label_get(obj, "%s", proc->state);
   elm_table_pack(table, label, 6, 0, 1, 1);

   label = _label_get(obj, "%s", proc->command);
   elm_table_pack(table, label, 7, 0, 1, 1);

   return table;
}

void stats_poll(Ecore_Thread *thread, Ui *ui)
{
   Eina_List *list_prev, *processes, *l, *l2;
   Process_Info *proc, *proc_prev;

   list_prev = NULL;

   while (1)
     {
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


        EINA_LIST_FREE(list_prev, proc)
          {
             free(proc->command);
             free(proc);
          }

        eina_list_free(list_prev);

        if (ecore_thread_check(thread))
          break;

        ecore_thread_feedback(thread, processes);
        sleep(10);
        list_prev = process_list_get();
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

static void
_thread_process_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Ui *ui;
   Eina_List *processes, *l;
   Process_Info *proc;
   Elm_Genlist_Item_Class *itc;

   ui = data;
   processes = msg;

   itc = elm_genlist_item_class_new();
   itc->item_style = "full";
   itc->func.text_get = NULL;
   itc->func.content_get = _list_content_get;
   itc->func.state_get = NULL;
   itc->func.del = _list_content_del;

   elm_genlist_clear(ui->list);

   EINA_LIST_FOREACH(processes, l, proc)
     {
        elm_genlist_item_append(ui->list, itc, proc, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }

   elm_genlist_realized_items_update(ui->list);

   elm_genlist_item_class_free(itc);
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

   ui->list = list = elm_genlist_add(parent);
   elm_genlist_mode_set(list, ELM_LIST_SCROLL);
   elm_genlist_select_mode_set(list, ELM_OBJECT_SELECT_MODE_NONE);
   elm_scroller_bounce_set(list, EINA_TRUE, EINA_TRUE);
   elm_scroller_policy_set(list, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(list);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Process Information");
   elm_box_pack_end(box, frame);
   evas_object_show(frame);
   elm_object_content_set(frame, list);
}
