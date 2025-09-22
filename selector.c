#include <asm-generic/ioctls.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "../euro/array.h"
#include "selector.h"

void get_termsize(struct winsize* out) {
    ioctl(0, TIOCGWINSZ, out);
}

void clear_rect(int top, int left, int bottom, int right) {
    printf("\x1b[%d;%d;%d;%d$z", top, left, bottom, right);
}

void draw(selector* s)
{
    struct winsize size;
    get_termsize(&size);
    s->width = size.ws_col;
    s->height = size.ws_row;

    int count = array_len(s->items);
    int min = s->height > count ? count : s->height;

    int bottom_gap = 1;

    int offset = 0;
    //add one because selected is 0-index, height is 1-index
    if (s->selected >= s->height - (1 + bottom_gap)) {
        offset = (s->selected + (1 + bottom_gap)) - s->height;
    }

    int text_area_width = s->width / 2;
    int preview_area_width = s->width - text_area_width;

    if (s->preview_gen != NULL) {
        printf("\x1b[s");
        clear_rect(1, text_area_width + 1, s->height, s->width);
        printf("\x1b[%d;%dH", 1, text_area_width + 1);
        struct selector_preview_info info = {
            .width = preview_area_width,
            .id = s->selected
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
            *(const char**)array_at(s->items, realPos));
        printf("\x1b[%d`|", text_area_width);
        if (realPos == s->selected) {
            printf("\x1b[0m");
        }
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

    s->searching = 0;
    return s;
}

selector_id_t selector_select(selector* s)
{
    struct termios old;
    tcgetattr(0, &old);
    struct termios new = old;

    new.c_cc[VMIN] = 1;
    new.c_cc[VTIME] = 0;
    new.c_lflag = ~(ECHO | ICANON);

    tcsetattr(0, TCSANOW, &new);
    printf("\x1b[?1049h\x1b[0H");

    draw(s);

    char ch;
    while ((ch = getchar()) != 0) {
        switch (ch) {
        case 'j':
            s->selected++;
            if (s->selected >= array_len(s->items)) {
                s->selected = 0;
            }
            break;
        case 'k':
            if (s->selected == 0) {
                s->selected = array_len(s->items) - 1;
            } else {
                s->selected--;
            }
            break;
        case 'g':
            s->selected = 0;
            break;
        case 'G':
            s->selected = array_len(s->items) - 1;
            break;
        // ctrl-c
        case 0x03:
        // enter
        case '\r':
        case 'q':
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
    free(s);
}

const char* selector_get_by_id(selector* s, selector_id_t id)
{
    return *(const char**)array_at(s->items, id);
}
