#include "editor.h"

// Processing input

static void parse_color(fz_context *ctx, vstack *stack, float out[3], val col)
{
  out[0] = val_number(ctx, val_array_get(ctx, stack, col, 0));
  out[1] = val_number(ctx, val_array_get(ctx, stack, col, 1));
  out[2] = val_number(ctx, val_array_get(ctx, stack, col, 2));
}

static bool lisp_truth_value(fz_context *ctx, vstack *t, val v)
{
  return !(val_is_name(v) && strcmp(val_as_name(ctx, t, v), "nil") == 0);
}

bool editor_parse(fz_context *ctx,
                  vstack *stack,
                  val command,
                  struct editor_command *out)
{
  if (!val_is_array(command))
  {
    fprintf(stderr, "[command] invalid (not an array)");
    return 0;
  }

  int len = val_array_length(ctx, stack, command);
  if (len == 0)
  {
    fprintf(stderr, "[command] invalid (empty array)");
    return 0;
  }

  const char *verb =
      val_as_name(ctx, stack, val_array_get(ctx, stack, command, 0));

  if (!verb)
  {
    fprintf(stderr, "[command] invalid (no verb)");
    return 0;
  }

  if (strcmp(verb, "open") == 0)
  {
    if (len != 3)
      goto arity;
    val path = val_array_get(ctx, stack, command, 1);
    val data = val_array_get(ctx, stack, command, 2);
    if (!val_is_string(path) || !val_is_string(data))
      goto arguments;
    *out = (struct editor_command){
        .tag = EDIT_OPEN,
        .open =
            {
                .path = val_string(ctx, stack, path),
                .data = val_string(ctx, stack, data),
                .length = val_string_length(ctx, stack, data),
            },

    };
  }
  else if (strcmp(verb, "close") == 0)
  {
    if (len != 2) goto arity;
    val path = val_array_get(ctx, stack, command, 1);
    if (!val_is_string(path))
      goto arguments;
    *out = (struct editor_command){
        .tag = EDIT_CLOSE,
        .close =
            {
                .path = val_string(ctx, stack, path),
            },

    };
  }
  else if (strcmp(verb, "change") == 0)
  {
    if (len != 5) goto arity;
    val path = val_array_get(ctx, stack, command, 1);
    val offset = val_array_get(ctx, stack, command, 2);
    val length = val_array_get(ctx, stack, command, 3);
    val data = val_array_get(ctx, stack, command, 4);
    if (!val_is_string(path) ||
        !val_is_number(offset) ||
        !val_is_number(length) ||
        !val_is_string(data))
      goto arguments;
    *out = (struct editor_command){
        .tag = EDIT_CHANGE,
        .change =
            {
                .path = val_string(ctx, stack, path),
                .offset = val_number(ctx, offset),
                .remove_length = val_number(ctx, length),
                .data = val_string(ctx, stack, data),
                .insert_length = val_string_length(ctx, stack, data),
            },
    };
  }
  else if (strcmp(verb, "theme") == 0)
  {
    if (len != 3) goto arity;
    val bg = val_array_get(ctx, stack, command, 1);
    val fg = val_array_get(ctx, stack, command, 2);
    out->tag = EDIT_THEME;
    parse_color(ctx, stack, out->theme.bg, bg);
    parse_color(ctx, stack, out->theme.fg, fg);
  }
  else if (strcmp(verb, "previous-page") == 0)
  {
    *out =
        (struct editor_command){.tag = EDIT_PREVIOUS_PAGE, .previous_page = {}};
  }
  else if (strcmp(verb, "next-page") == 0)
  {
    *out = (struct editor_command){.tag = EDIT_NEXT_PAGE, .next_page = {}};
  }
  else if (strcmp(verb, "move-window") == 0)
  {
    if (len != 5) goto arity;
    *out = (struct editor_command){
        .tag = EDIT_MOVE_WINDOW,
        .move_window = {
            .x = val_number(ctx, val_array_get(ctx, stack, command, 1)),
            .y = val_number(ctx, val_array_get(ctx, stack, command, 2)),
            .w = val_number(ctx, val_array_get(ctx, stack, command, 3)),
            .h = val_number(ctx, val_array_get(ctx, stack, command, 4)),
        }};
  }
  else if (strcmp(verb, "stay-on-top") == 0)
  {
    if (len != 2)
      goto arity;
    bool status =
        lisp_truth_value(ctx, stack, val_array_get(ctx, stack, command, 1));
    *out = (struct editor_command){.tag = EDIT_STAY_ON_TOP,
                                   .stay_on_top = {.status = status}};
  }
  else
  {
    fprintf(stderr, "[command] unknown verb: %s\n", verb);
    return 0;
  }
  return 1;

arity:
  fprintf(stderr, "[command] %s: invalid arity\n", verb);
  return 0;

arguments:
  fprintf(stderr, "[command] %s: invalid arguments\n", verb);
  return 0;
}

// Sending output

static void output_sexp_string(FILE *f, const char *ptr, int len)
{
  for (const char *lim = ptr + len; ptr < lim; ptr++)
  {
    char c = *ptr;
    switch (c)
    {
      case '\t':
        putc_unlocked('\\', f);
        c = 't';
        break;
      case '\r':
        putc_unlocked('\\', f);
        c = 'r';
        break;
      case '\n':
        c = 'n';
      case '"':
      case '\\':
        putc_unlocked('\\', f);
    }
    putc_unlocked(c, f);
  }
}

static const char *editor_info_buffer(enum EDITOR_INFO_BUFFER name)
{
  switch (name)
  {
    case BUF_LOG:
      return "log";

    case BUF_OUT:
      return "out";
  }
}

void editor_append(enum EDITOR_INFO_BUFFER name,
                   int pos,
                   const char *data,
                   int data_len)
{
  fprintf(stdout, "(append %s %d \"", editor_info_buffer(name), pos);
  output_sexp_string(stdout, data, data_len);
  fprintf(stdout, "\")\n");
}

void editor_truncate(enum EDITOR_INFO_BUFFER name, int length)
{
  fprintf(stdout, "(truncate %s %d)\n", editor_info_buffer(name), length);
}

void editor_flush(void)
{
  puts("(flush)\n");
}

void editor_synctex(const char *path, int path_len, int line, int column)
{
  fprintf(stdout, "(synctex \"");
  output_sexp_string(stdout, (const void *)path, path_len);
  fprintf(stdout, "\" %d %d)\n", line, column);
}

void editor_reset_sync(void)
{
  puts("(reset-sync)\n");
}
