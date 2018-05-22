#include "system.h"
#include "process.h"
#include "ui.h"
#include <stdio.h>

typedef enum
{
   SORT_BY_NONE,
   SORT_BY_PID,
   SORT_BY_UID,
   SORT_BY_NICE,
   SORT_BY_PRI,
   SORT_BY_CPU,
   SORT_BY_THREADS,
   SORT_BY_SIZE,
   SORT_BY_RSS,
   SORT_BY_CMD,
   SORT_BY_STATE,
   SORT_BY_CPU_USAGE,
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

   Evas_Object *btn_pid;
   Evas_Object *btn_uid;
   Evas_Object *btn_nice;
   Evas_Object *btn_pri;
   Evas_Object *btn_cpu;
   Evas_Object *btn_threads;
   Evas_Object *btn_size;
   Evas_Object *btn_rss;
   Evas_Object *btn_cmd;
   Evas_Object *btn_state;
   Evas_Object *btn_cpu_usage;

   Eina_Strbuf *fields[PROCESS_INFO_FIELDS];

   int          poll_delay;

   Sort_Type    sort_type;
   Eina_Bool    sort_reverse;
} Ui;

typedef struct Sys_Stats
{
   int    cpu_count;
   double cpu_usage;
   long   mem_total;
   long   mem_used;
} Sys_Stats;

typedef struct _Cpu_Use {
   pid_t    pid;
   uint64_t cpu_time;
} Pid_Cpu_Use;

static Eina_List *_list_procs_usage = NULL;

static Eina_Lock _lock;

static long _memory_total = 0;
static long _memory_used = 0;

static void
_thread_sys_stats(void *data, Ecore_Thread *thread)
{
   while (1)
     {
        Sys_Stats *stats_sys = malloc(sizeof(Sys_Stats));

        stats_sys->cpu_count = system_cpu_memory_get(&stats_sys->cpu_usage, &stats_sys->mem_total, &stats_sys->mem_used);

        _memory_total = stats_sys->mem_total >>= 10;
        _memory_used = stats_sys->mem_used >>= 10;

        ecore_thread_feedback(thread, stats_sys);
     }
}

static void
_thread_sys_stats_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Ui *ui = data;
   Sys_Stats *stats_sys = msg;

   elm_progressbar_value_set(ui->progress_cpu, (double) stats_sys->cpu_usage / 100);
   elm_progressbar_value_set(ui->progress_mem, (double) ((stats_sys->mem_total / 100.0) * stats_sys->mem_used) / 1000000);

   free(stats_sys);
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
_fields_append(Ui *ui, Process_Info *proc)
{
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_PID], "%d <br>", proc->pid);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_UID], "%d <br>", proc->uid);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_NICE], "%d <br>", proc->nice);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_PRI], "%d <br>", proc->priority);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_CPU], "%d <br>", proc->cpu_id);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_THREADS], "%d <br>", proc->numthreads);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_SIZE], "%u K<br>", proc->mem_size >> 10);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_RSS], "%u K<br>", proc->mem_rss >> 10);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_COMMAND], "%s <br>", proc->command);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_STATE], "%s <br>", proc->state);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_CPU_USAGE], "%.0f%% <br>", proc->cpu_usage);
}

static void
_fields_show(Ui *ui, Process_Info *proc)
{
   elm_object_text_set(ui->entry_pid, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_PID]));
   elm_object_text_set(ui->entry_uid, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_UID]));
   elm_object_text_set(ui->entry_nice, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_NICE]));
   elm_object_text_set(ui->entry_pri, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_PRI]));
   elm_object_text_set(ui->entry_cpu, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_CPU]));
   elm_object_text_set(ui->entry_threads, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_THREADS]));
   elm_object_text_set(ui->entry_size, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_SIZE]));
   elm_object_text_set(ui->entry_rss, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_RSS]));
   elm_object_text_set(ui->entry_cmd, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_COMMAND]));
   elm_object_text_set(ui->entry_state, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_STATE]));
   elm_object_text_set(ui->entry_cpu_usage, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_CPU_USAGE]));
}

static void
_fields_clear(Ui *ui)
{
   for (int i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        eina_strbuf_reset(ui->fields[i]);
     }
}

static void
_fields_free(Ui *ui)
{
   for (int i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        eina_strbuf_free(ui->fields[i]);
     }
}

static Eina_List *
_list_procs_sort(Ui *ui, Eina_List *list_procs)
{
   switch (ui->sort_type)
     {
        case SORT_BY_PID:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_pid);
        break;

        case SORT_BY_UID:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_uid);
        break;

        case SORT_BY_NICE:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_nice);
        break;

        case SORT_BY_PRI:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_pri);
        break;

        case SORT_BY_CPU:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_cpu);
        break;

        case SORT_BY_THREADS:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_threads);
        break;

        case SORT_BY_SIZE:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_size);
        break;

        case SORT_BY_RSS:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_rss);
        break;

        case SORT_BY_CMD:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_cmd);
        break;

        case SORT_BY_STATE:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_state);
        break;

        case SORT_BY_NONE:
        case SORT_BY_CPU_USAGE:
           list_procs = eina_list_sort(list_procs, eina_list_count(list_procs), _sort_by_cpu_usage);
        break;
     }

   if (ui->sort_reverse)
     list_procs = eina_list_reverse(list_procs);

   return list_procs;
}


static void
_thread_proc_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msg EINA_UNUSED)
{
   Ui *ui;
   Evas_Object *label;
   Eina_List *list_procs, *l, *l2;
   Process_Info *proc;
   Pid_Cpu_Use *usage;
   int i;
   static int poll_count = 0;

   eina_lock_take(&_lock);

   ui = data;

   list_procs = proc_info_all_get();

   EINA_LIST_FOREACH(list_procs, l, proc)
     {
        proc->cpu_usage = 0;
        EINA_LIST_FOREACH(_list_procs_usage, l2, usage)
          {
             if (usage->pid == proc->pid && proc->cpu_time > usage->cpu_time)
               {
                  proc->cpu_usage = (proc->cpu_time - usage->cpu_time) / ui->poll_delay;
                  break;
               }
          }
     }

   list_procs = _list_procs_sort(ui, list_procs);

   Eina_Bool show = EINA_FALSE;

   if (_list_procs_usage || !poll_count)
     show = EINA_TRUE;

   EINA_LIST_FREE(list_procs, proc)
     {
        if (show)
          _fields_append(ui, proc);

        Pid_Cpu_Use *use = malloc(sizeof(Pid_Cpu_Use));
        use->pid = proc->pid;
        use->cpu_time = proc->cpu_time;
        _list_procs_usage = eina_list_append(_list_procs_usage, use);

        free(proc);
     }

   if (list_procs)
     eina_list_free(list_procs);

   if (poll_count % 2)
     {
        EINA_LIST_FREE(_list_procs_usage, usage)
          {
             free(usage);
          }
        eina_list_free(_list_procs_usage);
        _list_procs_usage =  NULL;
     }

   if (show)
     {
        _fields_show(ui, proc);
        _fields_clear(ui);
     }

   poll_count++;

   eina_lock_release(&_lock);
}

static void
_ui_refresh(Ui *ui)
{
   _thread_proc_feedback_cb(ui, NULL, NULL);
}

static void
_thread_proc_stats(void *data, Ecore_Thread *thread)
{
   Ui *ui = data;

   while (1)
     {
        ecore_thread_feedback(thread, ui);
        if (ecore_thread_check(thread))
          break;

        sleep(ui->poll_delay);
     }
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
_btn_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_PID)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_pid, ui->sort_reverse);

   ui->sort_type = SORT_BY_PID;

   _ui_refresh(ui);
}

static void
_btn_uid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_UID)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_uid, ui->sort_reverse);

   ui->sort_type = SORT_BY_UID;

   _ui_refresh(ui);
}

static void
_btn_nice_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_NICE)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_nice, ui->sort_reverse);

   ui->sort_type = SORT_BY_NICE;

   _ui_refresh(ui);
}

static void
_btn_pri_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_PRI)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_pri, ui->sort_reverse);

   ui->sort_type = SORT_BY_PRI;

   _ui_refresh(ui);
}

static void
_btn_cpu_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CPU)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_cpu, ui->sort_reverse);

   ui->sort_type = SORT_BY_CPU;

   _ui_refresh(ui);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CPU_USAGE)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_cpu_usage, ui->sort_reverse);

   ui->sort_type = SORT_BY_CPU_USAGE;

   _ui_refresh(ui);
}



static void
_btn_threads_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_THREADS)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_threads, ui->sort_reverse);

   ui->sort_type = SORT_BY_THREADS;

   _ui_refresh(ui);
}

static void
_btn_size_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_SIZE)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_size, ui->sort_reverse);

   ui->sort_type = SORT_BY_SIZE;

   _ui_refresh(ui);
}

static void
_btn_rss_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_RSS)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_rss, ui->sort_reverse);

   ui->sort_type = SORT_BY_RSS;

   _ui_refresh(ui);
}

static void
_btn_cmd_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CMD)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_cmd, ui->sort_reverse);

   ui->sort_type = SORT_BY_CMD;

   _ui_refresh(ui);
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_STATE)
     ui->sort_reverse = !ui->sort_reverse;

   _icon_sort_set(ui->btn_state, ui->sort_reverse);

   ui->sort_type = SORT_BY_STATE;

   _ui_refresh(ui);
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
   ui->poll_delay = 2;
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

   table = elm_table_add(parent);
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

   ui->btn_pid = button = elm_button_add(parent);
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

   ui->btn_uid = button = elm_button_add(parent);
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

   ui->btn_nice = button = elm_button_add(parent);
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

   ui->btn_pri = button = elm_button_add(parent);
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

   ui->btn_cpu = button = elm_button_add(parent);
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

   ui->btn_threads = button = elm_button_add(parent);
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

   ui->btn_size = button = elm_button_add(parent);
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

   ui->btn_rss = button = elm_button_add(parent);
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

   ui->btn_cmd = button = elm_button_add(parent);
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

   ui->btn_state = button = elm_button_add(parent);
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

   ui->btn_cpu_usage = button = elm_button_add(parent);
   _icon_sort_set(button, EINA_TRUE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "CPU %");
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

   evas_object_smart_callback_add(ui->btn_pid, "clicked", _btn_pid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_uid, "clicked", _btn_uid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_nice, "clicked", _btn_nice_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_pri, "clicked", _btn_pri_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cpu, "clicked", _btn_cpu_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_threads, "clicked", _btn_threads_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_size, "clicked", _btn_size_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_rss, "clicked", _btn_rss_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cmd, "clicked", _btn_cmd_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_state, "clicked", _btn_state_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cpu_usage, "clicked", _btn_cpu_usage_clicked_cb, ui);

   for (int i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        ui->fields[i] = eina_strbuf_new();
     }

   ecore_thread_feedback_run(_thread_sys_stats, _thread_sys_stats_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
   ecore_thread_feedback_run(_thread_proc_stats, _thread_proc_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
}
