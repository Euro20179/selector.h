#include <asm-generic/ioctls.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>

#include "../euro/array.h"
#include "selector.h"

static struct termios old;

void clean_up(int sig) {
    tcsetattr(0, TCSADRAIN, &old);
}

void get_termsize(struct winsize* out)
{
    ioctl(0, TIOCGWINSZ, out);
}

void clear_rect(int top, int left, int bottom, int right)
{
    printf("\x1b[%d;%d;%d;%d$z", top, left, bottom, right);
}

selector_id_t get_id_from_selected(selector* s, size_t selected)
{
    if (selected == -1) {
        selected = s->selected;
    }

    size_t passed = 0;
    for (int i = 0; i < array_len(s->items); i++) {
        if (s->item_ids[i] != -1) {
            passed++;
            //passed is 1-index, selected is 0 index
            if (passed - 1 == selected) {
                return s->item_ids[i];
            }
        }
    }
    return 0;
}

void draw(selector* s)
{

    size_t ow = s->width;
    size_t oh = s->height;

    struct winsize size;
    get_termsize(&size);
    s->width = size.ws_col;
    s->height = size.ws_row;

    if(s->width != ow || s->height != oh) {
        printf("\x1b[0H\x1b[2J");
    }

    int count = array_len(s->visible_items);
    int min = s->height > count ? count : s->height;

    int bottom_gap = 1;
    if(min != s->height) {
        bottom_gap = 0;
    }

    int offset = 0;
    // add one because selected is 0-index, height is 1-index
    if (s->selected >= min - (1 + bottom_gap)) {
        offset = (s->selected + (1 + bottom_gap)) - min;
    }

    int text_area_width = s->width / 2;
    int preview_area_width = s->width - text_area_width;

    if (s->preview_gen != NULL) {
        printf("\x1b[s");
        clear_rect(1, text_area_width + 1, s->height, s->width);
        printf("\x1b[%d;%dH", 1, text_area_width + 1);

        struct selector_preview_info info = {
            .width = preview_area_width,
            .id = get_id_from_selected(s, -1),
        };
        string* toprint = (*s->preview_gen)(info);

        for (size_t i = 0; i < string_len(toprint); i++) {
            char ch = string_at(toprint, i);

            switch (ch) {
            case '\r':
                printf("\x1b[%d`", text_area_width + 1);
                break;
            case '\n':
                printf("\x1b[B\x1b[%d`", text_area_width + 1);
                break;
            default:
                printf("%c", ch);
            }
        }
        string_del2(toprint);
        printf("\x1b[u");
    }

    for (int i = 0; i < min - bottom_gap; i++) {
        int realPos = i + offset;

        if (realPos == s->selected) {
            printf("\x1b[32m");
        }

        clear_rect(i + 1, 1, i + 2, text_area_width);
        printf(
            "\x1b[%d;%dH%-.*s", i + 1, 1,
            (int)text_area_width,
            *(const char**)array_at(s->visible_items, realPos));
        printf("\x1b[%d`|", text_area_width);
        if (realPos == s->selected) {
            printf("\x1b[0m");
        }
    }

    if (string_len(s->current_search) != 0) {
        printf("\x1b[%zu;1H", s->height);
        string_to_cstr_buf_create(buf, (*(s->current_search)));
        string_to_cstr(s->current_search, buf);
        printf("\x1b[34m/%-.*s\x1b[0m", text_area_width, buf);
    }
}

selector* selector_new2(struct selector_action_handlers handlers, array* items)
{
    selector* s = malloc(sizeof(selector));
    s->on_hover = handlers.on_hover;
    s->preview_gen = handlers.preview_gen;
    s->items = items;
    s->height = 5;
    s->width = 80;
    s->selected = 0;

    s->item_ids = malloc(sizeof(size_t) * array_len(items));

    s->searching = 0;
    s->current_search = string_new2(0);
    s->visible_items = array_new2(0, sizeof(const char**));
    for (size_t i = 0; i < array_len(s->items); i++) {
        array_append(s->visible_items, array_at(s->items, i));
        s->item_ids[i] = i;
    }
    return s;
}

int process_char(selector* s, char ch)
{
    if (s->searching) {
        if (ch == '\r') {
            s->searching = 0;
            s->selected = 0;

            array_del2(s->visible_items);

            // repopulate array so it's accurate
            for (size_t i = 0; i < array_len(s->items); i++) {
                s->item_ids[i] = i;
            }

            s->visible_items = array_new2(0, sizeof(const char**));
            string_to_cstr_buf_create(buf, (*(s->current_search)));
            string_to_cstr(s->current_search, buf);
            for (size_t i = 0; i < array_len(s->items); i++) {
                const char** item = array_at(s->items, i);
                if (cstr_includes(*item, buf)) {
                    array_append(s->visible_items, item);
                } else {
                    s->item_ids[i] = -1;
                }
            }

            printf("\x1b[s\x1b[0H\x1b[2J\x1b[u");

        } else if (ch == '\x7F') {
            if (string_len(s->current_search) >= 1) {
                string_slice_suffix(s->current_search, 1);
            }
        } else {
            string_concat_char(s->current_search, ch);
        }
        return 0;
    }

    switch (ch) {
    case 'j':
        s->selected++;
        if (s->selected >= array_len(s->visible_items)) {
            s->selected = 0;
        }
        break;
    case 'k':
        if (s->selected == 0) {
            s->selected = array_len(s->visible_items) - 1;
        } else {
            s->selected--;
        }
        break;
    case 'g':
        s->selected = 0;
        break;
    case 'G':
        s->selected = array_len(s->visible_items) - 1;
        break;
    // ctrl-c
    case 0x03:
    // enter
    case '\r':
    case 'q':
        return 1;
    case '/':
        string_clear(s->current_search);
        s->searching = 1;
        break;
    }
    return 0;
}

selector_id_t selector_select(selector* s)
{
    tcgetattr(0, &old);
    struct termios new = old;

    new.c_cc[VMIN] = 1;
    new.c_cc[VTIME] = 0;
    new.c_lflag = ~(ECHO | ICANON);

    tcsetattr(0, TCSANOW, &new);

    signal(SIGSEGV, clean_up);
    signal(SIGINT, clean_up);
    signal(SIGFPE, clean_up);
    signal(SIGTERM, clean_up);
    signal(SIGABRT, clean_up);
    signal(SIGABRT, clean_up);
    signal(SIGILL, clean_up);

    printf("\x1b[?1049h\x1b[0H\x1b[2J");

    draw(s);

    char ch;
    while ((ch = getchar()) != 0) {
        if (process_char(s, ch) == 1) {
            goto done;
        }
        draw(s);
    }

done:
    printf("\x1b[?1049l");
    tcsetattr(0, TCSADRAIN, &old);

    return s->selected;
}

void selector_del2(selector* s)
{
    string_del2(s->current_search);
    array_del2(s->visible_items);
    free(s->item_ids);
    free(s);
}

const char* selector_get_by_id(selector* s, selector_id_t id)
{
    size_t real_id = get_id_from_selected(s, id);
    return *(const char**)array_at(s->items, real_id);
}
