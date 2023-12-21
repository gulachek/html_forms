#include <html-forms.h>

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

int loop(sqlite3 *db, html_connection con, const char *render_path);

int main() {
  sqlite3 *db;
  if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  html_connection con = html_connection_alloc();
  if (!con) {
    fprintf(stderr, "Failed to allocate html connection\n");
    return 1;
  }

  if (!html_connect(con)) {
    // TODO - print error in connection
    return 1;
  }

  const char *render_path = tmpnam(NULL);
  int ret = loop(db, con, render_path);

  sqlite3_close(db);
  remove(render_path);
  return ret;
}

int view_tasks(sqlite3 *db, html_connection con, const char *render_path,
               html_form *pform) {
  FILE *f = fopen(render_path, "w");
  if (!f)
    return 0;

  fprintf(f, "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<title> Todo Items </title>"
             "<link rel=\"stylesheet\" href=\"~/main.css\" />"
             "</head>"
             "<body>"
             "<h1> Todo items </h1>"
             "<ul class=\"todo-items\">");

  sqlite3_stmt *select;
  const char *sql = "SELECT id, title, priority, due_date "
                    "FROM tasks ORDER BY due_date ASC, priority DESC";

  if (sqlite3_prepare_v2(db, sql, -1, &select, NULL)) {
    fprintf(stderr, "Failed to prepare SELECT: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  while (sqlite3_step(select) == SQLITE_ROW) {
    int id = sqlite3_column_int(select, 0);
    const unsigned char *title = sqlite3_column_text(select, 1);
    int priority = sqlite3_column_int(select, 2);
    int due_date = sqlite3_column_int(select, 3);

    fprintf(f,
            "<li>"
            "<span class=\"title\"> %s </span>"
            "</li>",
            title);
  }

  if (sqlite3_finalize(select)) {
    fprintf(stderr, "Failed to finalize SELECT: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  fprintf(f, "</ul>"
             "</body>"
             "</html>");

  fflush(f);
  if (!html_upload_file(con, "/view.html", render_path)) {
    fprintf(stderr, "Failed to upload /view.html\n");
    goto fail;
  }

  if (!html_navigate(con, "/view.html")) {
    fprintf(stderr, "Failed to navigate to /view.html\n");
    goto fail;
  }

  html_form_release(pform);
  if (html_read_form(con, pform)) {
    goto fail;
  }

success:
  fclose(f);
  return 1;

fail:
  fclose(f);
  return 0;
}

int init_db(sqlite3 *db) {
  const char *sql = "CREATE TABLE tasks ("
                    " id INTEGER PRIMARY KEY,"
                    " title TEXT,"
                    " description TEXT,"
                    " priority INTEGER,"
                    " due_date INTEGER"
                    ");"
                    "INSERT INTO tasks (title) VALUES (\"Test\");";

  char *errmsg;
  if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
    fprintf(stderr, "Error initializing database: %s\n", errmsg);
    return 0;
  }

  return 1;
}

int loop(sqlite3 *db, html_connection con, const char *render_path) {
  if (!init_db(db))
    return 1;

  if (!html_upload_dir(con, "/", DOCROOT_PATH))
    return 1;

  html_form form = NULL;
  const char *action = "view";

  while (1) {
    if (strcmp(action, "view") == 0) {
      if (!view_tasks(db, con, render_path, &form)) {
        return 1;
      }
    } else {
      fprintf(stderr, "Unrecognized action '%s'\n", action);
      return 1;
    }

    action = html_form_value_of(form, "action");
  }

  html_form_release(&form);
}
