#include <yed/plugin.h>

void comment_toggle(int n_args, char **args);
int  comment_toggle_line(yed_frame *frame, yed_line *line, int row);
void toggle_line(yed_frame *frame, yed_line *line, int row, const char *start, const char *end);
void comment_line(yed_frame *frame, int row, const char *start, const char *end);
void uncomment_line(yed_frame *frame, int row, const char *start, const char *end);

int yed_plugin_boot(yed_plugin *self) {
    YED_PLUG_VERSION_CHECK();
    yed_plugin_set_command(self, "comment-toggle", comment_toggle);
    return 0;
}

void comment_toggle(int n_args, char **args) {
    int         err;
    yed_frame  *frame;
    yed_buffer *buff;
    yed_line   *line;
    int         save_col,
                row,
                r1, c1, r2, c2;

    if (n_args != 0) {
        yed_cerr("expected 0 arguments but got %d", n_args);
        return;
    }

    if (!ys->active_frame) {
        yed_cerr("no active frame");
        return;
    }

    err = 0;

    frame = ys->active_frame;

    if (!frame->buffer) {
        yed_cerr("active frame has no buffer");
        return;
    }

    buff = frame->buffer;

    yed_start_undo_record(frame, buff);

    save_col = frame->cursor_col;
    yed_set_cursor_within_frame(frame, frame->cursor_line, 1);

    if (buff->has_selection) {
        yed_range_sorted_points(&buff->selection, &r1, &c1, &r2, &c2);

        for (row = r1; row <= r2; row += 1) {
            line = yed_buff_get_line(buff, row);
            err  = comment_toggle_line(frame, line, row);
        }

        YEXE("select-off");
    } else {
        line = yed_buff_get_line(buff, frame->cursor_line);
        err  = comment_toggle_line(frame, line, frame->cursor_line);
    }

    yed_set_cursor_within_frame(frame, frame->cursor_line, save_col);

    if (err) {
        yed_cerr("Comment style unknown for %s", yed_get_ft_name(frame->buffer->ft));
        yed_cancel_undo_record(frame, buff);
    } else {
        yed_end_undo_record(frame, buff);
    }
}

int comment_toggle_line(yed_frame *frame, yed_line *line, int row) {
    int         err;
    char       *style;
    char        var_buff[256];
    char       *cpy;
    int         len;
    const char *start;
    const char *end;

    err = 0;

    snprintf(var_buff, sizeof(var_buff),
             "%s-comment-style",
             yed_get_ft_name(frame->buffer->ft));

    if ((style = yed_get_var(var_buff)) == NULL) {
        err = 1;
        goto out;
    }

    cpy   = strdup(style);
    start = strtok(cpy, " ");
    end   = strtok(NULL, " ");

    if (start == NULL) {
        err = 1;
        goto out_free;
    }

    toggle_line(frame, line, row, start, end);

out_free:;
    free(cpy);
out:;
    return err;
}


void toggle_line(yed_frame *frame, yed_line *line, int row, const char *start, const char *end) {
    int        line_len;
    int        start_len;
    int        end_len;
    int        i;
    yed_glyph *g;

    line_len  = line->visual_width;

    if (line_len == 0) { return; }

    start_len = strlen(start);
    end_len   = end == NULL ? 0 : strlen(end);

    /* Are we uncommenting? */
    if (line_len >= 2 + start_len + end_len) {
        for (i = 0; i < start_len; i += 1) {
            g = yed_line_col_to_glyph(line, 1 + i);
            if (g->c != start[i]) { goto comment; }
        }
        g = yed_line_col_to_glyph(line, 1 + i);
        if (g->c != ' ') { goto comment; }

        if (end != NULL) {
            for (i = 0; i < end_len; i += 1) {
                g = yed_line_col_to_glyph(line, line_len - i);
                if (g->c != end[end_len - i - 1]) { goto comment; }
            }
            g = yed_line_col_to_glyph(line, line_len - i);
            if (g->c != ' ') { goto comment; }
        }

        uncomment_line(frame, row, start, end);
        return;
    }

comment:;
    comment_line(frame, row, start, end);
}

void comment_line(yed_frame *frame, int row, const char *start, const char *end) {
    int start_len;
    int end_len;
    int i;

    start_len = strlen(start);
    end_len   = end == NULL ? 0 : strlen(end);

    yed_insert_into_line(frame->buffer, row, 1, GLYPH(" "));
    for (i = 0; i < start_len; i += 1) {
        yed_insert_into_line(frame->buffer, row, 1, GLYPH(&start[start_len - i - 1]));
    }

    if (end != NULL) {
        yed_append_to_line(frame->buffer, row, GLYPH(" "));
        for (i = 0; i < end_len; i += 1) {
            yed_append_to_line(frame->buffer, row, GLYPH(&end[i]));
        }
    }
}

void uncomment_line(yed_frame *frame, int row, const char *start, const char *end) {
    int start_len;
    int end_len;
    int i;

    start_len = strlen(start);
    end_len   = end == NULL ? 0 : strlen(end);

    for (i = 0; i < start_len + 1; i += 1) {
        yed_delete_from_line(frame->buffer, row, 1);
    }

    if (end != NULL) {
        for (i = 0; i < end_len + 1; i += 1) {
            yed_pop_from_line(frame->buffer, row);
        }
    }
}
