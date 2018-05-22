#include "system.h"
#include "process.h"
#include "ui.h"
#include <stdio.h>

typedef enum
{
   SORT_NONE,
   SORT_PID,
   SORT_UID,
   SORT_NICE,
   SORT_PRI,
   SORT_CPU,
   SORT_THREADS,
   SORT_SIZE,
   SORT_RSS,
   SORT_CMD,
   SORT_STATE,
   SORT_CPU_USAGE,
} Sort_Type;

typedef struct Ui
{
   Evas_Object *progress_cpu;
   Evas_Object *progress_mem;

   Evas_Object *entry_pid;
   Evas_Object *entry_uid;
   Evas_Object *entry_nice;
   Evas_Object *entry_pri;
   Evas_Object *entry_cpu;
   Evas_Object *entry_threads;
   Evas_Object *entry_size;
   Evas_Object *entry_rss;
   Evas_Object *entry_cmd;
   Evas_Object *entry_state;
   Evas_Object *entry_cpu_usage;

   Evas_Object *button_pid;
   Evas_Object *button_uid;
   Evas_Object *button_nice;
   Evas_Object *button_pri;
   Evas_Object *button_cpu;
   Evas_Object *button_threads;
   Evas_Object *button_size;
   Evas_Object *button_rss;
   Evas_Object *button_cmd;
   Evas_Object *button_state;
   Evas_Object *button_cpu_usage;

   int          poll_delay;

   Evas_Object *table;

   Sort_Type    sort;
   Eina_Bool    sort_reverse;
} Ui;

typedef struct Sys_Info
{
   int    cpu_count;
   double cpu_usage;
   long   mem_total;
   long   mem_used;
} Sys_Info;

typedef struct _Cpu_Use {
   pid_t pid;
   uint64_t cpu_time;
} Process_Cpu_Use;

static Eina_List *_process_usage = NULL;

static Eina_Lock _lock;

static long _memory_total = 0;
static long _memory_used = 0;

void
_stats_poll(Ecore_Thread *thread, Ui *ui)
{
   while (1)
     {
        ecore_thread_feedback(thread, ui);
        if (ecore_thread_check(thread))
          break;
        sleep(ui->poll_delay);
     }
}

static void
_thread_system_run(void *data, Ecore_Thread *thread)
{
   while (1)
     {
        Sys_Info *results = malloc(sizeof(Sys_Info));

        results->cpu_count = system_cpu_memory_get(&results->cpu_usage, &results->mem_total, &results->mem_used);

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

static int
_sort_by_pid(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->pid - inf2->pid;
}

static int
_sort_by_uid(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->uid - inf2->uid;
}

static int
_sort_by_nice(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->nice - inf2->nice;
}

static int
_sort_by_pri(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->priority - inf2->priority;
}

static int
_sort_by_cpu(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->cpu_id - inf2->cpu_id;
}

static int
_sort_by_threads(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->numthreads - inf2->numthreads;
}


static int
_sort_by_size(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->mem_size - inf2->mem_size;
}


static int
_sort_by_rss(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->mem_rss - inf2->mem_rss;
}

static int
_sort_by_cpu_usage(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->cpu_usage - inf2->cpu_usage;
}

static int
_sort_by_cmd(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcasecmp(inf1->command, inf2->command);
}

static int
_sort_by_state(const void *p1, const void *p2)
{
   const Process_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcmp(inf1->state, inf2->state);
}


static void
_thread_process_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msg EINA_UNUSED)
{
   Ui *ui;
   Evas_Object *label;
   Eina_List *procs, *l, *l2;
   Process_Info *proc;
   Process_Cpu_Use *usage;
   Eina_Strbuf *fields[PROCESS_INFO_FIELDS];
   int i;
   static int poll_count = 0;

   eina_lock_take(&_lock);

   ui = data;

   for (i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        fields[i] = eina_strbuf_new();
     }

   procs = proc_info_all_get();

   EINA_LIST_FOREACH(procs, l, proc)
     {
        proc->cpu_usage = 0;
        EINA_LIST_FOREACH(_process_usage, l2, usage)
          {
             if (usage->pid == proc->pid && proc->cpu_time > usage->cpu_time)
               {
                  proc->cpu_usage = (proc->cpu_time - usage->cpu_time) / ui->poll_delay;
                  break;
               }
          }
     }

   switch (ui->sort)
     {
        case SORT_PID:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_pid);
        break;

        case SORT_UID:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_uid);
        break;

        case SORT_NICE:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_nice);
        break;

        case SORT_PRI:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_pri);
        break;

        case SORT_CPU:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_cpu);
        break;

        case SORT_THREADS:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_threads);
        break;

        case SORT_SIZE:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_size);
        break;

        case SORT_RSS:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_rss);
        break;

        case SORT_CMD:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_cmd);
        break;

        case SORT_STATE:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_state);
        break;

        case SORT_NONE:
        case SORT_CPU_USAGE:
           procs = eina_list_sort(procs, eina_list_count(procs), _sort_by_cpu_usage);
        break;
     }

   if (ui->sort_reverse)
     procs = eina_list_reverse(procs);

   Eina_Bool show = EINA_FALSE;

   if (_process_usage || !poll_count)
     show = EINA_TRUE;

   EINA_LIST_FREE(procs, proc)
     {
        if (show)
          {
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_PID], "%d <br>", proc->pid);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_UID], "%d <br>", proc->uid);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_NICE], "%d <br>", proc->nice);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_PRI], "%d <br>", proc->priority);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_CPU], "%d <br>", proc->cpu_id);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_THREADS], "%d <br>", proc->numthreads);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_SIZE], "%u K<br>", proc->mem_size >> 10);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_RSS], "%u K<br>", proc->mem_rss >> 10);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_COMMAND], "%s <br>", proc->command);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_STATE], "%s <br>", proc->state);
             eina_strbuf_append_printf(fields[PROCESS_INFO_FIELD_CPU_USAGE], "%.0f%% <br>", proc->cpu_usage);
          }

        Process_Cpu_Use *use = malloc(sizeof(Process_Cpu_Use));
        use->pid = proc->pid;
        use->cpu_time = proc->cpu_time;
        _process_usage = eina_list_append(_process_usage, use);

        free(proc);
     }

   if (procs)
     eina_list_free(procs);

   if (poll_count % 2)
     {
        EINA_LIST_FREE(_process_usage, usage)
          {
             free(usage);
          }
        eina_list_free(_process_usage);
        _process_usage =  NULL;
     }

   if (show)
     {
        elm_object_text_set(ui->entry_pid, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_PID]));
        elm_object_text_set(ui->entry_uid, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_UID]));
        elm_object_text_set(ui->entry_nice, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_NICE]));
        elm_object_text_set(ui->entry_pri, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_PRI]));
        elm_object_text_set(ui->entry_cpu, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_CPU]));
        elm_object_text_set(ui->entry_threads, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_THREADS]));
        elm_object_text_set(ui->entry_size, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_SIZE]));
        elm_object_text_set(ui->entry_rss, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_RSS]));
        elm_object_text_set(ui->entry_cmd, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_COMMAND]));
        elm_object_text_set(ui->entry_state, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_STATE]));
        elm_object_text_set(ui->entry_cpu_usage, eina_strbuf_string_get(fields[PROCESS_INFO_FIELD_CPU_USAGE]));
     }

   for (i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        eina_strbuf_free(fields[i]);
     }

   eina_lock_release(&_lock);

   poll_count++;
}

static void
_update_ui(Ui *ui)
{
   _thread_process_feedback_cb(ui, NULL, NULL);
}

static void
_thread_process_run(void *data, Ecore_Thread *thread)
{
   Ui *ui = data;

   _stats_poll(thread, ui);
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

static void
_icon_sort_set(Evas_Object *button, Eina_Bool reverse)
{
   Evas_Object *icon = elm_icon_add(button);
   if (reverse)
     elm_icon_standard_set(icon, "go-down");
   else
     elm_icon_standard_set(icon, "go-up");

   elm_object_part_content_set(button, "icon", icon);
}

static void
_button_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_PID)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_pid, ui->sort_reverse);

   ui->sort = SORT_PID;
   _update_ui(ui);
}

static void
_button_uid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_UID)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_uid, ui->sort_reverse);

   ui->sort = SORT_UID;
   _update_ui(ui);
}

static void
_button_nice_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_NICE)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_nice, ui->sort_reverse);

   ui->sort = SORT_NICE;
   _update_ui(ui);
}

static void
_button_pri_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_PRI)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_pri, ui->sort_reverse);

   ui->sort = SORT_PRI;
   _update_ui(ui);
}

static void
_button_cpu_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_CPU)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_cpu, ui->sort_reverse);

   ui->sort = SORT_CPU;
   _update_ui(ui);
}

static void
_button_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_CPU_USAGE)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_cpu_usage, ui->sort_reverse);

   ui->sort = SORT_CPU_USAGE;
   _update_ui(ui);
}



static void
_button_threads_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_THREADS)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_threads, ui->sort_reverse);

   ui->sort = SORT_THREADS;
   _update_ui(ui);
}

static void
_button_size_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_SIZE)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_size, ui->sort_reverse);

   ui->sort = SORT_SIZE;
   _update_ui(ui);
}

static void
_button_rss_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_RSS)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_rss, ui->sort_reverse);

   ui->sort = SORT_RSS;
   _update_ui(ui);
}

static void
_button_cmd_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_CMD)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_cmd, ui->sort_reverse);

   ui->sort = SORT_CMD;
   _update_ui(ui);
}

static void
_button_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort == SORT_STATE)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->button_state, ui->sort_reverse);

   ui->sort = SORT_STATE;
   _update_ui(ui);
}

void
ui_add(Evas_Object *parent)
{
   Evas_Object *box, *hbox, *frame, *table;
   Evas_Object *progress_cpu, *progress_mem, *list;
   Evas_Object *button, *entry, *icon;
   Ecore_Thread *thread;
   Ui *ui;

   ui = calloc(1, sizeof(Ui));
   ui->poll_delay = 5;
   ui->sort_reverse = EINA_TRUE;

   eina_lock_new(&_lock);

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

   ui->button_pid = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "PID");
   evas_object_show(button);
   elm_table_pack(table, button, 0, 0, 1, 1);

   ui->entry_pid = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 0, 1, 1, 1);

   ui->button_uid = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "UID");
   evas_object_show(button);
   elm_table_pack(table, button, 1, 0, 1, 1);

   ui->entry_uid = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 1, 1, 1, 1);

   ui->button_nice = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Nice");
   evas_object_show(button);
   elm_table_pack(table, button, 2, 0, 1, 1);

   ui->entry_nice = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 2, 1, 1, 1);

   ui->button_pri = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Priority");
   evas_object_show(button);
   elm_table_pack(table, button, 3, 0, 1, 1);

   ui->entry_pri = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 3, 1, 1, 1);

   ui->button_cpu = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "CPU #");
   evas_object_show(button);
   elm_table_pack(table, button, 4, 0, 1, 1);

   ui->entry_cpu = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 4, 1, 1, 1);

   ui->button_threads = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Threads");
   evas_object_show(button);
   elm_table_pack(table, button, 5, 0, 1, 1);

   ui->entry_threads = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 5, 1, 1, 1);

   ui->button_size = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Size");
   evas_object_show(button);
   elm_table_pack(table, button, 6, 0, 1, 1);

   ui->entry_size = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 6, 1, 1, 1);

   ui->button_rss = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Res");
   evas_object_show(button);
   elm_table_pack(table, button, 7, 0, 1, 1);

   ui->entry_rss = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 7, 1, 1, 1);

   ui->button_cmd = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Command");
   evas_object_show(button);
   elm_table_pack(table, button, 8, 0, 1, 1);

   ui->entry_cmd = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 8, 1, 1, 1);

   ui->button_state = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "State");
   evas_object_show(button);
   elm_table_pack(table, button, 9, 0, 1, 1);

   ui->entry_state = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   elm_entry_line_wrap_set(entry, 1);
   evas_object_show(entry);
   elm_table_pack(table, entry, 9, 1, 1, 1);

   ui->button_cpu_usage = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_TRUE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "CPU Use");
   evas_object_show(button);
   elm_table_pack(table, button, 10, 0, 1, 1);

   ui->entry_cpu_usage = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='font=default:style=default size=12 align=center'");
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   elm_entry_line_wrap_set(entry, 1);
   evas_object_show(entry);
   elm_table_pack(table, entry, 10, 1, 1, 1);

   evas_object_smart_callback_add(ui->button_pid, "clicked", _button_pid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_uid, "clicked", _button_uid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_nice, "clicked", _button_nice_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_pri, "clicked", _button_pri_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_cpu, "clicked", _button_cpu_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_threads, "clicked", _button_threads_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_size, "clicked", _button_size_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_rss, "clicked", _button_rss_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_cmd, "clicked", _button_cmd_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_state, "clicked", _button_state_clicked_cb, ui);
   evas_object_smart_callback_add(ui->button_cpu_usage, "clicked", _button_cpu_usage_clicked_cb, ui);

   ecore_thread_feedback_run(_thread_system_run, _thread_system_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
   ecore_thread_feedback_run(_thread_process_run, _thread_process_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
}
