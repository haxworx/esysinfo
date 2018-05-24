#include "system.h"
#include "process.h"
#include "ui.h"
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>

static Eina_Lock _lock;

static long _memory_total = 0;
static long _memory_used = 0;

static void
_thread_sys_stats(void *data, Ecore_Thread *thread)
{
   while (1)
     {
        Sys_Stats *sys = malloc(sizeof(Sys_Stats));

        sys->cpu_count = system_cpu_memory_get(&sys->cpu_usage, &sys->mem_total, &sys->mem_used);

        _memory_total = sys->mem_total >>= 10;
        _memory_used = sys->mem_used >>= 10;

        ecore_thread_feedback(thread, sys);
     }
}

static void
_thread_sys_stats_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Ui *ui = data;
   Sys_Stats *sys = msg;

   if (ecore_thread_check(thread))
     return;

   elm_progressbar_value_set(ui->progress_cpu, (double)sys->cpu_usage / 100);
   elm_progressbar_value_set(ui->progress_mem, (double)((sys->mem_total / 100.0) * sys->mem_used) / 1000000);

   free(sys);
}

static int
_sort_by_pid(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->pid - inf2->pid;
}

static int
_sort_by_uid(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->uid - inf2->uid;
}

static int
_sort_by_nice(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->nice - inf2->nice;
}

static int
_sort_by_pri(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->priority - inf2->priority;
}

static int
_sort_by_cpu(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->cpu_id - inf2->cpu_id;
}

static int
_sort_by_threads(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->numthreads - inf2->numthreads;
}

static int
_sort_by_size(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->mem_size - inf2->mem_size;
}

static int
_sort_by_rss(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->mem_rss - inf2->mem_rss;
}

static int
_sort_by_cpu_usage(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->cpu_usage - inf2->cpu_usage;
}

static int
_sort_by_cmd(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcasecmp(inf1->command, inf2->command);
}

static int
_sort_by_state(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcmp(inf1->state, inf2->state);
}

static void
_fields_append(Ui *ui, Proc_Stats *proc)
{
   // FIXME: hiding self from the list until more efficient.
   // It's not too bad but it pollutes a lovely list.
   if (ui->program_pid == proc->pid)
     return;

   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_PID], "%d <br>", proc->pid);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_UID], "%d <br>", proc->uid);
   /*
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_NICE], "%d <br>", proc->nice);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_PRI], "%d <br>", proc->priority);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_CPU], "%d <br>", proc->cpu_id);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_THREADS], "%d <br>", proc->numthreads);
   */
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_SIZE], "%ld K<br>", proc->mem_size);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_RSS], "%ld K<br>", proc->mem_rss);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_COMMAND], "%s <br>", proc->command);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_STATE], "%s <br>", proc->state);
   eina_strbuf_append_printf(ui->fields[PROCESS_INFO_FIELD_CPU_USAGE], "%.0f%% <br>", proc->cpu_usage);
}

static void
_fields_show(Ui *ui, Proc_Stats *proc)
{
   elm_object_text_set(ui->entry_pid, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_PID]));
   elm_object_text_set(ui->entry_uid, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_UID]));
   /*
   elm_object_text_set(ui->entry_nice, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_NICE]));
   elm_object_text_set(ui->entry_pri, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_PRI]));
   elm_object_text_set(ui->entry_cpu, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_CPU]));
   elm_object_text_set(ui->entry_threads, eina_strbuf_string_get(ui->fields[PROCESS_INFO_FIELD_THREADS]));
   */
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
_conversions_apply(Ui *ui, Proc_Stats *proc)
{
   proc->mem_size >>= 10;
   proc->mem_rss >>= 10;
}

static void
_thread_overview_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msg EINA_UNUSED)
{
   Ui *ui;
   Eina_List *list_procs, *l;
   Proc_Stats *proc;

   if (ecore_thread_check(thread))
     return;

   eina_lock_take(&_lock);

   ui = data;

   list_procs = proc_info_all_get();

   EINA_LIST_FOREACH (list_procs, l, proc)
     {
        int64_t time_prev = ui->cpu_times[proc->pid];
        _conversions_apply(ui, proc);
        proc->cpu_usage = 0;
        if (!ui->first_run && proc->cpu_time > time_prev)
          {
             proc->cpu_usage = (proc->cpu_time - time_prev) / ui->poll_delay;
          }
     }

   list_procs = _list_procs_sort(ui, list_procs);

   EINA_LIST_FREE (list_procs, proc)
     {
        _fields_append(ui, proc);
        ui->first_run = EINA_FALSE;
        ui->cpu_times[proc->pid] = proc->cpu_time;

        free(proc);
     }

   if (list_procs)
     eina_list_free(list_procs);

   _fields_show(ui, proc);
   _fields_clear(ui);

   eina_lock_release(&_lock);
}

static void
_ui_refresh(Ui *ui)
{
   _thread_overview_feedback_cb(ui, NULL, NULL);
}

static void
_thread_overview(void *data, Ecore_Thread *thread)
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
_btn_icon_state_set(Evas_Object *button, Eina_Bool reverse)
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

   _btn_icon_state_set(ui->btn_pid, ui->sort_reverse);

   ui->sort_type = SORT_BY_PID;

   _ui_refresh(ui);
}

static void
_btn_uid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_UID)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_uid, ui->sort_reverse);

   ui->sort_type = SORT_BY_UID;

   _ui_refresh(ui);
}

static void
_btn_nice_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_NICE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_nice, ui->sort_reverse);

   ui->sort_type = SORT_BY_NICE;

   _ui_refresh(ui);
}

static void
_btn_pri_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_PRI)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_pri, ui->sort_reverse);

   ui->sort_type = SORT_BY_PRI;

   _ui_refresh(ui);
}

static void
_btn_cpu_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CPU)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_cpu, ui->sort_reverse);

   ui->sort_type = SORT_BY_CPU;

   _ui_refresh(ui);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CPU_USAGE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_cpu_usage, ui->sort_reverse);

   ui->sort_type = SORT_BY_CPU_USAGE;

   _ui_refresh(ui);
}

static void
_btn_threads_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_THREADS)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_threads, ui->sort_reverse);

   ui->sort_type = SORT_BY_THREADS;

   _ui_refresh(ui);
}

static void
_btn_size_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_SIZE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_size, ui->sort_reverse);

   ui->sort_type = SORT_BY_SIZE;

   _ui_refresh(ui);
}

static void
_btn_rss_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_RSS)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_rss, ui->sort_reverse);

   ui->sort_type = SORT_BY_RSS;

   _ui_refresh(ui);
}

static void
_btn_cmd_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CMD)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_cmd, ui->sort_reverse);

   ui->sort_type = SORT_BY_CMD;

   _ui_refresh(ui);
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_STATE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_state, ui->sort_reverse);

   ui->sort_type = SORT_BY_STATE;

   _ui_refresh(ui);
}

static void
_btn_quit_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ecore_main_loop_quit();

   elm_exit();
}

static void
_btn_about_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;
   Evas_Object *win;

   ui = data;
   win = ui->win;

   printf("(c) Copyright 2018. Alastair Poole <netstar@gmail.com>\n");
}

static void
_thread_proc_pid_stats(void *data, Ecore_Thread *thread)
{
   Ui *ui = data;

   while (1)
     {
        ecore_thread_feedback(thread, ui);
        if (ecore_thread_check(thread))
          return;

        sleep(30);
     }
}

static void
_thread_proc_pid_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Proc_Stats *proc;
   Eina_List *proc_list;
   Ui *ui = msg;
   char buf[64];

   proc_list = proc_info_all_get();
   proc_list = eina_list_sort(proc_list, eina_list_count(proc_list), _sort_by_pid);

   elm_list_clear(ui->list_pid);

   EINA_LIST_FREE(proc_list, proc)
     {
        snprintf(buf, sizeof(buf), "%d", proc->pid);
        elm_list_item_append(ui->list_pid, buf, NULL, NULL, NULL, NULL);
        free(proc);
     }

   if (proc_list)
     eina_list_free(proc_list);
}

Eina_Bool
_list_pid_poll(void *data)
{
   Ui *ui;
   struct passwd *pwd_entry;
   Proc_Stats *proc;
   double cpu_usage;
   int64_t time_prev;
   char buf[1024];

   ui = data;

   proc = proc_info_by_pid(ui->selected_pid);
   if (!proc)
     {
        _thread_proc_pid_feedback_cb(NULL, NULL, ui);

        return ECORE_CALLBACK_CANCEL;
     }

   elm_object_text_set(ui->entry_pid_cmd, proc->command);
   snprintf(buf, sizeof(buf), "%d", proc->pid);

   pwd_entry = getpwuid(proc->uid);
   if (pwd_entry)
     elm_object_text_set(ui->entry_pid_user, pwd_entry->pw_name);

   elm_object_text_set(ui->entry_pid_pid, buf);
   snprintf(buf, sizeof(buf), "%d", proc->uid);
   elm_object_text_set(ui->entry_pid_uid, buf);
   snprintf(buf, sizeof(buf), "%d", proc->cpu_id);
   elm_object_text_set(ui->entry_pid_cpu, buf);
   snprintf(buf, sizeof(buf), "%d", proc->numthreads);
   elm_object_text_set(ui->entry_pid_threads, buf);
   snprintf(buf, sizeof(buf), "%ld bytes", proc->mem_size);
   elm_object_text_set(ui->entry_pid_size, buf);
   snprintf(buf, sizeof(buf), "%ld bytes", proc->mem_rss);
   elm_object_text_set(ui->entry_pid_rss, buf);
   snprintf(buf, sizeof(buf), "%d", proc->nice);
   elm_object_text_set(ui->entry_pid_nice, buf);
   snprintf(buf, sizeof(buf), "%d", proc->priority);
   elm_object_text_set(ui->entry_pid_pri, buf);
   snprintf(buf, sizeof(buf), "%s", proc->state);
   elm_object_text_set(ui->entry_pid_state, buf);

   time_prev = ui->cpu_times[proc->pid];

   cpu_usage = 0;
   if (!ui->first_run && proc->cpu_time > time_prev)
     {
        cpu_usage = (proc->cpu_time - time_prev) / ui->poll_delay;
     }

   snprintf(buf, sizeof(buf), "%.0f%%", cpu_usage);
   elm_object_text_set(ui->entry_pid_cpu_usage, buf);

   free(proc);

   return ECORE_CALLBACK_RENEW;
}

static void
_list_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Elm_Object_Item *it;
   Ui *ui;
   const char *text;

   ui = data;

   it = elm_list_selected_item_get(obj);

   text = elm_object_item_text_get(it);

   if (ui->timer_pid)
     {
        ecore_timer_del(ui->timer_pid);
        ui->timer_pid = NULL;
     }

   ui->selected_pid = atoi(text);

   _list_pid_poll(ui);

   ui->timer_pid = ecore_timer_add(ui->poll_delay, _list_pid_poll, ui);
}

static void
_btn_start_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGCONT);
}

static void
_btn_stop_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGSTOP);
}

static void
_btn_kill_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGKILL);
}

static void
_ui_process_list_add(Evas_Object *parent, Ui *ui)
{
   Evas_Object *box, *hbox, *frame, *table;
   Evas_Object *progress, *button, *entry, *icon;
   Evas_Object *panel, *list, *label;

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

   ui->progress_cpu = progress = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   elm_progressbar_unit_format_set(progress, "%1.2f%%");
   elm_object_content_set(frame, progress);
   evas_object_show(progress);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "System Memory");
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   ui->progress_mem = progress = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   elm_progressbar_unit_format_function_set(progress, _progress_mem_format_cb, _progress_mem_format_free_cb);
   elm_object_content_set(frame, progress);
   evas_object_show(progress);

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
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0.5);//EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "System Overview");
   elm_box_pack_end(box, frame);
   evas_object_show(frame);
   elm_object_content_set(frame, scroller);

   ui->btn_pid = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
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
   _btn_icon_state_set(button, EINA_FALSE);
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

/*
   ui->btn_nice = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
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
   _btn_icon_state_set(button, EINA_FALSE);
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
   _btn_icon_state_set(button, EINA_FALSE);
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
   _btn_icon_state_set(button, EINA_FALSE);
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
*/
   ui->btn_size = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
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
   _btn_icon_state_set(button, EINA_FALSE);
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
   _btn_icon_state_set(button, EINA_FALSE);
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
   _btn_icon_state_set(button, EINA_FALSE);
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
   _btn_icon_state_set(button, EINA_TRUE);
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

   /* End of system information and process list overview */

   panel = elm_panel_add(parent);
   evas_object_size_hint_weight_set(panel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(panel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_panel_orient_set(panel, ELM_PANEL_ORIENT_LEFT);
   elm_panel_toggle(panel);
   elm_object_content_set(parent, panel);
   evas_object_show(panel);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   elm_object_content_set(panel, hbox);
   evas_object_show(hbox);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, 0.2, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "PID");
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   ui->list_pid = list = elm_list_add(frame);
   evas_object_size_hint_weight_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(list);
   elm_object_content_set(frame, list);
   evas_object_smart_callback_add(ui->list_pid, "selected", _list_clicked_cb, ui);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Process Statistics");
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, 0.5, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_padding_set(table, 3, 3);
   evas_object_show(table);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_show(scroller);
   elm_object_content_set(scroller, table);
   elm_object_content_set(frame, scroller);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "Name:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 0, 1, 1);

   ui->entry_pid_cmd = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 0, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "PID:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 1, 1, 1);

   ui->entry_pid_pid = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 1, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "Username:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 2, 1, 1);

   ui->entry_pid_user = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 2, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "UID:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 3, 1, 1);

   ui->entry_pid_uid = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 3, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "CPU #:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 4, 1, 1);

   ui->entry_pid_cpu = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 4, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "Threads:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 5, 1, 1);

   ui->entry_pid_threads = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 5, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "Total memory:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 6, 1, 1);

   ui->entry_pid_size = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 6, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, " Reserved memory:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 7, 1, 1);

   ui->entry_pid_rss = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 7, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "Nice:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 8, 1, 1);

   ui->entry_pid_nice = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 8, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "Priority:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 9, 1, 1);

   ui->entry_pid_pri = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 9, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "State:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 10, 1, 1);

   ui->entry_pid_state = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 10, 1, 1);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "CPU %:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 11, 1, 1);

   ui->entry_pid_cpu_usage = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 11, 1, 1);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);
   elm_table_pack(table, hbox, 1, 12, 1, 1);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Stop Process");
   elm_box_pack_end(hbox, button);
   evas_object_show(button);
   evas_object_smart_callback_add(button, "clicked", _btn_stop_clicked_cb, ui);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Start Process");
   elm_box_pack_end(hbox, button);
   evas_object_smart_callback_add(button, "clicked", _btn_start_clicked_cb, ui);
   evas_object_show(button);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Kill Process");
   elm_box_pack_end(hbox, button);
   evas_object_show(button);
   evas_object_smart_callback_add(button, "clicked", _btn_kill_clicked_cb, ui);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);
   elm_box_pack_end(box, hbox);

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(box, EINA_TRUE);
   elm_box_pack_end(hbox, box);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, 0.2, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0);
   elm_object_text_set(button, "Quit");
   elm_box_pack_end(hbox, button);
   evas_object_show(button);
   evas_object_smart_callback_add(button, "clicked", _btn_quit_clicked_cb, ui);
}

void
ui_add(Evas_Object *parent)
{
   Ecore_Thread *thread;
   Ui *ui;

   ui = calloc(1, sizeof(Ui));
   ui->win = parent;
   ui->first_run = EINA_TRUE;
   ui->poll_delay = 2;
   ui->sort_reverse = EINA_TRUE;
   ui->selected_pid = -1;
   ui->program_pid = getpid();

   memset(ui->cpu_times, 0, PID_MAX * sizeof(int64_t));

   for (int i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        ui->fields[i] = eina_strbuf_new();
     }

   eina_lock_new(&_lock);

   _ui_process_list_add(parent, ui);

   ecore_thread_feedback_run(_thread_sys_stats, _thread_sys_stats_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
   ecore_thread_feedback_run(_thread_overview, _thread_overview_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
   ecore_thread_feedback_run(_thread_proc_pid_stats, _thread_proc_pid_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
}

