#include <html-forms.h>

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

int loop(sqlite3 *db, html_connection *con, const char *render_path);

int main() {
  sqlite3 *db;
  if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  html_connection *con;

  if (!html_connect(&con)) {
    // TODO - print error in connection
    fprintf(stderr, "Failed to make html connection: %s\n", html_errmsg(con));
    return 1;
  }

  const char *render_path = tmpnam(NULL);
  int ret = loop(db, con, render_path);

  sqlite3_close(db);
  remove(render_path);
  return ret;
}

void print_header(FILE *f) {
  fprintf(f, "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<title> Todo Items </title>"
             "<script src=\"/html/forms.js\"></script>"
             "<link rel=\"stylesheet\" href=\"~/main.css\" />"
             "</head>"
             "<body>");
}

void print_footer(FILE *f) { fprintf(f, "</body></html>"); }

int view_tasks(sqlite3 *db, html_connection *con, const char *render_path,
               html_form *pform) {
  FILE *f = fopen(render_path, "w");
  if (!f)
    return 0;

  print_header(f);
  fprintf(f, "<h1> Todo items </h1>"
             "<form class=\"toolbar\">"
             "<button name=\"action\" value=\"add\"> New Task </button>"
             "</form>"
             "<ul class=\"todo-items\">");

  sqlite3_stmt *select;
  const char *sql = "SELECT id, title, priority, due_date "
                    "FROM tasks ORDER BY LENGTH(due_date) DESC, due_date ASC, "
                    "priority DESC";

  if (sqlite3_prepare_v2(db, sql, -1, &select, NULL)) {
    fprintf(stderr, "Failed to prepare SELECT: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  char esc_title[1024];
  char esc_date[11]; // really should be yyyy-mm-dd (10, no escape)

  while (sqlite3_step(select) == SQLITE_ROW) {
    int id = sqlite3_column_int(select, 0);
    const unsigned char *title = sqlite3_column_text(select, 1);
    int priority = sqlite3_column_int(select, 2);
    const unsigned char *due_date = sqlite3_column_text(select, 3);

    if (html_escape(esc_title, sizeof(esc_title), (const char *)title) >
        sizeof(esc_title)) {
      fprintf(stderr, "Title too big: %s\n", title);
      goto fail;
    }

    if (html_escape(esc_date, sizeof(esc_date), (const char *)due_date) >
        sizeof(esc_date)) {
      fprintf(stderr, "Date too big: %s\n", due_date);
      goto fail;
    }

    const char *date_class =
        (due_date && strlen((const char *)due_date) > 0) ? "" : " empty";

    const char *bullet_img = "~/icons/normal.svg";

    switch (priority) {
    case 0:
      bullet_img = "~/icons/lower.svg";
      break;
    case 2:
      bullet_img = "~/icons/important.svg";
      break;
    default:
      break;
    }

    fprintf(f,
            "<li>"
            "<form class=\"todo-line\">"
            "<input type=\"hidden\" name=\"id\" value=\"%d\" />"
            "<img class=\"todo-bullet\" src=\"%s\"></img>"
            "<span class=\"title\"> %s </span>"
            "<span class=\"due-date%s\"> (%s) </span>"
            "<button name=\"action\" value=\"edit\"> Edit </button>"
            "<button name=\"action\" value=\"delete\"> Done </button>"
            "</form>"
            "</li>",
            id, bullet_img, esc_title, date_class, esc_date);
  }

  if (sqlite3_finalize(select)) {
    fprintf(stderr, "Failed to finalize SELECT: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  fprintf(f, "</ul>");
  print_footer(f);

  fflush(f);
  if (!html_upload_file(con, "/view.html", render_path)) {
    fprintf(stderr, "Failed to upload /view.html: %s\n", html_errmsg(con));
    goto fail;
  }

  if (!html_navigate(con, "/view.html")) {
    fprintf(stderr, "Failed to navigate to /view.html: %s\n", html_errmsg(con));
    goto fail;
  }

  html_form_release(pform);
  if (html_read_form(con, pform)) {
    fprintf(stderr, "Failed to read form: %s\n", html_errmsg(con));
    goto fail;
  }

success:
  fclose(f);
  return 1;

fail:
  fclose(f);
  return 0;
}

int edit_task(int task, sqlite3 *db, html_connection *con,
              const char *render_path, html_form *pform) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT title, description, priority, due_date "
                    "FROM tasks WHERE id = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) {
    fprintf(stderr, "Failed to prepare SELECT: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  if (sqlite3_bind_int(stmt, 1, task)) {
    fprintf(stderr, "Failed to bind task: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  if (sqlite3_step(stmt) != SQLITE_ROW) {
    fprintf(stderr, "Failed to execute SELECT: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  const unsigned char *title = sqlite3_column_text(stmt, 0);
  const unsigned char *desc = sqlite3_column_text(stmt, 1);
  int priority = sqlite3_column_int(stmt, 2);
  const unsigned char *due_date = sqlite3_column_text(stmt, 3);

  FILE *f = fopen(render_path, "w");
  if (!f)
    return 0;

  char esc_title[1024];
  char esc_desc[4096];
  char esc_date[512];

  if (html_escape(esc_title, sizeof(esc_title), (const char *)title) >
      sizeof(esc_title)) {
    fprintf(stderr, "Title too big: %s\n", title);
    goto fail;
  }

  if (html_escape(esc_desc, sizeof(esc_desc), (const char *)desc) >
      sizeof(esc_desc)) {
    fprintf(stderr, "Description too big: %s\n", desc);
    goto fail;
  }

  if (html_escape(esc_date, sizeof(esc_date), (const char *)due_date) >
      sizeof(esc_date)) {
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

  print_header(f);
  fprintf(f,
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

  print_footer(f);

  if (sqlite3_finalize(stmt)) {
    fprintf(stderr, "Failed to finalize SELECT: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  fflush(f);
  if (!html_upload_file(con, "/edit.html", render_path)) {
    fprintf(stderr, "Failed to upload /edit.html: %s\n", html_errmsg(con));
    goto fail;
  }

  if (!html_navigate(con, "/edit.html")) {
    fprintf(stderr, "Failed to navigate to /edit.html: %s\n", html_errmsg(con));
    goto fail;
  }

  html_form_release(pform);
  if (html_read_form(con, pform)) {
    fprintf(stderr, "Failed to read form: %s\n", html_errmsg(con));
    goto fail;
  }

  const char *action = html_form_value_of(*pform, "action");
  if (strcmp(action, "save") != 0) {
    fprintf(stderr, "Unknown edit action: %s\n", action);
    goto fail;
  }

  const char *saveSql = "UPDATE tasks SET title=?"
                        " ,description=?"
                        " ,priority=?"
                        " ,due_date=?"
                        " WHERE id=?";

  if (sqlite3_prepare_v2(db, saveSql, -1, &stmt, NULL)) {
    fprintf(stderr, "Failed to prepare UPDATE: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  const char *form_title = html_form_value_of(*pform, "title");
  if (sqlite3_bind_text(stmt, 1, form_title, -1, SQLITE_STATIC)) {
    fprintf(stderr, "Failed to bind title: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  const char *form_desc = html_form_value_of(*pform, "description");
  if (sqlite3_bind_text(stmt, 2, form_desc, -1, SQLITE_STATIC)) {
    fprintf(stderr, "Failed to bind description: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  int form_priority = atoi(html_form_value_of(*pform, "priority"));
  if (sqlite3_bind_int(stmt, 3, form_priority)) {
    fprintf(stderr, "Failed to bind priority: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  const char *form_date = html_form_value_of(*pform, "due-date");
  if (sqlite3_bind_text(stmt, 4, form_date, -1, SQLITE_STATIC)) {
    fprintf(stderr, "Failed to bind description: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  if (sqlite3_bind_int(stmt, 5, task)) {
    fprintf(stderr, "Failed to bind task: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  sqlite3_step(stmt);
  if (sqlite3_finalize(stmt)) {
    fprintf(stderr, "Failed to finalize UPDATE: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

success:
  fclose(f);
  return 1;

fail:
  fclose(f);
  sqlite3_finalize(stmt);
  return 0;
}

int init_db(sqlite3 *db) {
  const char *sql = "CREATE TABLE tasks ("
                    " id INTEGER PRIMARY KEY,"
                    " title TEXT,"
                    " description TEXT,"
                    " priority INTEGER DEFAULT 1,"
                    " due_date TEXT"
                    ");"
                    "INSERT INTO tasks (title) VALUES (\"Test\");";

  char *errmsg;
  if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
    fprintf(stderr, "Error initializing database: %s\n", errmsg);
    return 0;
  }

  return 1;
}

int create_task(sqlite3 *db, int *pid) {
  char *errmsg;
  if (sqlite3_exec(db, "INSERT INTO tasks DEFAULT VALUES", NULL, NULL,
                   &errmsg)) {
    fprintf(stderr, "Failed to create task: %s\n", errmsg);
    return 0;
  }

  *pid = sqlite3_last_insert_rowid(db);
  return 1;
}

int delete_task(sqlite3 *db, int task) {
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM tasks WHERE id = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) {
    fprintf(stderr, "Failed to prepare DELETE: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  if (sqlite3_bind_int(stmt, 1, task)) {
    fprintf(stderr, "Failed to bind task: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
  }

  if (sqlite3_finalize(stmt)) {
    fprintf(stderr, "Failed to finalize DELETE: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  return 1;

fail:
  sqlite3_finalize(stmt);
  return 0;
}

int loop(sqlite3 *db, html_connection *con, const char *render_path) {
  if (!init_db(db))
    return 1;

  if (!html_upload_dir(con, "/", DOCROOT_PATH)) {
    fprintf(stderr, "Failed to upload docroot: %s\n", html_errmsg(con));
    return 1;
  }

  html_form form = NULL;
  const char *action = "view";
  int selected_task = -1;

  while (1) {
    if (strcmp(action, "view") == 0) {
      if (!view_tasks(db, con, render_path, &form)) {
        return 1;
      }

      action = html_form_value_of(form, "action");
      const char *id = html_form_value_of(form, "id");
      if (id) {
        selected_task = atoi(id);
      }

    } else if (strcmp(action, "add") == 0) {
      if (!create_task(db, &selected_task))
        return 1;

      action = "edit";
    } else if (strcmp(action, "edit") == 0) {
      if (!edit_task(selected_task, db, con, render_path, &form)) {
        return 1;
      }

      action = "view";
    } else if (strcmp(action, "delete") == 0) {
      if (!delete_task(db, selected_task))
        return 1;

      action = "view";
    } else {
      fprintf(stderr, "Unrecognized action '%s'\n", action);
      return 1;
    }
  }

  html_form_release(&form);
}
