#include <html_forms.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern const char *docroot;

struct task {
  int id;
  int is_null;
  char title[64];
  char description[256];
  int priority;
  char due_date[16];
};

#define MAX_TASKS 16
static struct task db[MAX_TASKS];

int loop(html_connection *con);

int hprintf(html_connection *con, const char *fmt, ...) {
  static char dummy;
  static char *buf = &dummy;
  static size_t bufsz = sizeof(char);

  while (1) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, bufsz, fmt, args);
    va_end(args);

    if (ret < 0) {
      return ret;
    } else if (ret < bufsz) {
      if (!html_upload_stream_write(con, buf, ret)) {
        return -1;
      }

      return ret;
    } else {
      bufsz = ret + 1;
      buf = malloc(bufsz);
      if (!buf)
        return -1;
    }
  }

  return -1;
}

int main() {
  html_connection *con;

  if (!html_connect(&con)) {
    // TODO - print error in connection
    fprintf(stderr, "Failed to make html connection: %s\n", html_errmsg(con));
    return 1;
  }

  int ret = loop(con);

  html_disconnect(con);
  return ret;
}

void print_header(html_connection *con) {
  hprintf(
      con,
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<title> Todo Items </title>"
      "<script src=\"/html/forms.js\"></script>"
      "<link rel=\"stylesheet\" href=\"~/main.css\" />"
      "</head>"
      "<body>"
      "<svg class=\"svg-defs\">"
      /*<!--!Font Awesome Free 6.5.1 by @fontawesome - https : //
         fontawesome.com License - https://fontawesome.com/license/free
         // Copyright 2023 Fonticons, Inc.-->*/
      "<def>"
      "<path id=\"icon-normal\" d=\"M256 512A256 256 0 1 0 256 0a256 256 0 1 "
      "0 0 512z\" />"
      "<path id=\"icon-important\" d=\"M256 512A256 256 0 1 0 256 0a256 256 0 "
      "1 0 0 512zm0-384c13.3 0 24 10.7 24 24V264c0 13.3-10.7 24-24 "
      "24s-24-10.7-24-24V152c0-13.3 10.7-24 24-24zM224 352a32 32 0 1 1 64 0 32 "
      "32 0 1 1 -64 0z\"/>"
      "<path id=\"icon-lower\" d=\"M256 0a256 256 0 1 0 0 512A256 256 0 1 0 "
      "256 0zM376.9 294.6L269.8 394.5c-3.8 3.5-8.7 5.5-13.8 "
      "5.5s-10.1-2-13.8-5.5L135.1 294.6c-4.5-4.2-7.1-10.1-7.1-16.3c0-12.3 "
      "10-22.3 22.3-22.3l57.7 0 0-96c0-17.7 14.3-32 32-32l32 0c17.7 0 32 14.3 "
      "32 32l0 96 57.7 0c12.3 0 22.3 10 22.3 22.3c0 6.2-2.6 12.1-7.1 16.3z\"/>"
      "</def>"
      "</svg>");
}

void print_footer(html_connection *con) { hprintf(con, "</body></html>"); }

int view_tasks(html_connection *con, html_form **pform) {
  if (!html_upload_stream_open(con, "/view.html"))
    return 0;

  print_header(con);
  hprintf(con, "<h1> Todo items </h1>"
               "<form class=\"toolbar\">"
               "<button name=\"action\" value=\"add\"> New Task </button>"
               "</form>"
               "<ul class=\"todo-items\">");

  char esc_title[1024];
  char esc_date[11]; // really should be yyyy-mm-dd (10, no escape)

  for (int i = 0; i < MAX_TASKS; ++i) {
    const struct task *t = &db[i];
    if (t->is_null)
      continue;

    int id = i;
    const char *title = t->title;
    int priority = t->priority;
    const char *due_date = t->due_date;

    if (html_escape(esc_title, sizeof(esc_title), title) > sizeof(esc_title)) {
      fprintf(stderr, "Title too big: %s\n", title);
      goto fail;
    }

    if (html_escape(esc_date, sizeof(esc_date), due_date) > sizeof(esc_date)) {
      fprintf(stderr, "Date too big: %s\n", due_date);
      goto fail;
    }

    const char *date_class =
        (due_date && strlen((const char *)due_date) > 0) ? "" : " empty";

    const char *bullet_href = "icon-normal";

    switch (priority) {
    case 0:
      bullet_href = "icon-lower";
      break;
    case 2:
      bullet_href = "icon-important";
      break;
    default:
      break;
    }

    hprintf(con,
            "<li>"
            "<form class=\"todo-line\">"
            "<input type=\"hidden\" name=\"id\" value=\"%d\" />"
            "<svg viewBox=\"0 0 512 512\" width=\"16\" height=\"16\"><use "
            "href=\"#%s\" /></svg>"
            "<span class=\"title\"> %s </span>"
            "<span class=\"due-date%s\"> (%s) </span>"
            "<button name=\"action\" value=\"edit\"> Edit </button>"
            "<button name=\"action\" value=\"delete\"> Done </button>"
            "</form>"
            "</li>",
            id, bullet_href, esc_title, date_class, esc_date);
  }

  hprintf(con, "</ul>");
  print_footer(con);

  if (!html_upload_stream_close(con)) {
    fprintf(stderr, "Failed to upload /view.html: %s\n", html_errmsg(con));
    goto fail;
  }

  if (!html_navigate(con, "/view.html")) {
    fprintf(stderr, "Failed to navigate to /view.html: %s\n", html_errmsg(con));
    goto fail;
  }

  html_form_free(*pform);
  if (!html_form_read(con, pform)) {
    fprintf(stderr, "Failed to read form: %s\n", html_errmsg(con));
    goto fail;
  }

success:
  return 1;

fail:
  return 0;
}

int edit_task(int task, html_connection *con, html_form **pform) {
  if (task < 0 || task >= MAX_TASKS)
    return 0;

  struct task *t = &db[task];
  if (t->is_null)
    return 0;

  const char *title = t->title;
  const char *desc = t->description;
  int priority = t->priority;
  const char *due_date = t->due_date;

  if (!html_upload_stream_open(con, "/edit.html"))
    return 0;

  char esc_title[1024];
  char esc_desc[4096];
  char esc_date[512];

  if (html_escape(esc_title, sizeof(esc_title), title) > sizeof(esc_title)) {
    fprintf(stderr, "Title too big: %s\n", title);
    goto fail;
  }

  if (html_escape(esc_desc, sizeof(esc_desc), desc) > sizeof(esc_desc)) {
    fprintf(stderr, "Description too big: %s\n", desc);
    goto fail;
  }

  if (html_escape(esc_date, sizeof(esc_date), due_date) > sizeof(esc_date)) {
    fprintf(stderr, "Due date too big: %s\n", due_date);
    goto fail;
  }

  const char *important_selected = "";
  const char *normal_selected = "";
  const char *lower_selected = "";
  switch (priority) {
  case 0:
    lower_selected = "selected";
    break;
  case 1:
    normal_selected = "selected";
    break;
  case 2:
    important_selected = "selected";
    break;
  default:
    break;
  }

  print_header(con);
  hprintf(con,
          "<form>"
          "<h1> %s </h1>"
          "<div class=\"toolbar\">"
          "<button name=\"action\" value=\"save\"> Save </button>"
          "</div>"
          "<label> Title: <input type=\"text\" name=\"title\" "
          "value=\"%s\"/></label>"
          "<br />"
          "<label> Description: <textarea "
          "name=\"description\">%s</textarea></label>"
          "<br />"
          "<label> Priority: <select name=\"priority\">"
          "<option %s value=\"2\"> Important </option>"
          "<option %s value=\"1\"> Normal </option>"
          "<option %s value=\"0\"> Lower </option>"
          "</select></label>"
          "<br />"
          "<label> Due Date: <input type=\"date\" name=\"due-date\" "
          "value=\"%s\"/></label>"
          "</form>",

          esc_title, esc_title, esc_desc, important_selected, normal_selected,
          lower_selected, esc_date);

  print_footer(con);

  if (!html_upload_stream_close(con)) {
    fprintf(stderr, "Failed to upload /edit.html: %s\n", html_errmsg(con));
    goto fail;
  }

  if (!html_navigate(con, "/edit.html")) {
    fprintf(stderr, "Failed to navigate to /edit.html: %s\n", html_errmsg(con));
    goto fail;
  }

  html_form_free(*pform);
  if (!html_form_read(con, pform)) {
    fprintf(stderr, "Failed to read form: %s\n", html_errmsg(con));
    goto fail;
  }

  const char *action = html_form_value_of(*pform, "action");
  if (strcmp(action, "save") != 0) {
    fprintf(stderr, "Unknown edit action: %s\n", action);
    goto fail;
  }

  const char *form_title = html_form_value_of(*pform, "title");
  if (form_title) {
    strlcpy(t->title, form_title, sizeof(t->title));
  } else {
    t->title[0] = '\0';
  }

  const char *form_desc = html_form_value_of(*pform, "description");
  if (form_desc) {
    strlcpy(t->description, form_desc, sizeof(t->description));
  } else {
    t->description[0] = '\0';
  }

  int form_priority = atoi(html_form_value_of(*pform, "priority"));
  if (0 <= form_priority && form_priority <= 2) {
    t->priority = form_priority;
  }

  const char *form_date = html_form_value_of(*pform, "due-date");
  if (form_date) {
    strlcpy(t->due_date, form_date, sizeof(t->due_date));
  } else {
    t->due_date[0] = '\0';
  }

success:
  return 1;

fail:
  return 0;
}

void init_task(struct task *t) {
  t->is_null = 0;
  t->title[0] = '\0';
  t->description[0] = '\0';
  t->priority = 1;
  t->due_date[0] = '\0';
}

int init_db() {

  for (int i = 0; i < MAX_TASKS; ++i) {
    db[i].id = i;
    db[i].is_null = 1;
  }

  init_task(&db[0]);
  strcpy(db[0].title, "Test");

  return 1;
}

int create_task(int *pid) {
  for (int i = 0; i < MAX_TASKS; ++i) {
    struct task *t = &db[i];
    if (t->is_null) {
      init_task(t);
      *pid = i;
      return 1;
    }
  }

  return 0;
}

int delete_task(int task) {
  if (task < 0 || task >= MAX_TASKS)
    return 0;

  if (db[task].is_null)
    return 0;

  db[task].is_null = 1;
  return 1;
}

int loop(html_connection *con) {
  if (!init_db())
    return 1;

  if (!html_upload_dir(con, "/", docroot)) {
    fprintf(stderr, "Failed to upload docroot: %s\n", html_errmsg(con));
    return 1;
  }

  html_form *form = NULL;
  const char *action = "view";
  int selected_task = -1;

  while (1) {
    if (strcmp(action, "view") == 0) {
      if (!view_tasks(con, &form)) {
        return 1;
      }

      action = html_form_value_of(form, "action");
      const char *id = html_form_value_of(form, "id");
      if (id) {
        selected_task = atoi(id);
      }

    } else if (strcmp(action, "add") == 0) {
      if (!create_task(&selected_task))
        return 1;

      action = "edit";
    } else if (strcmp(action, "edit") == 0) {
      if (!edit_task(selected_task, con, &form)) {
        return 1;
      }

      action = "view";
    } else if (strcmp(action, "delete") == 0) {
      if (!delete_task(selected_task))
        return 1;

      action = "view";
    } else {
      fprintf(stderr, "Unrecognized action '%s'\n", action);
      return 1;
    }
  }

  html_form_free(form);
}
