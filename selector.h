#pragma once

#include <stddef.h>
#include "../euro/array.h"
#include "../euro/string.h"

typedef size_t selector_id_t;

struct selector_preview_info {
    selector_id_t id;
    size_t width;
};

struct selector_action_handlers {
    void(*on_hover)(selector_id_t);
    string*(*preview_gen)(struct selector_preview_info);
};

typedef struct {
    void(*on_hover)(selector_id_t);
    ///the caller MUST call string_new2 on the string to return
    ///selector will handle calling string_del2
    string*(*preview_gen)(struct selector_preview_info);
    array* items;

    size_t height;
    size_t width;
    selector_id_t selected;
} selector;

///allocates a new selector
///caller MUST call selector_del2 to free the selector
///array MUST be a const char** array
selector* selector_new2(struct selector_action_handlers, array*);

void selector_del2(selector*);

selector_id_t selector_select(selector*);

const char* selector_get_by_id(selector*, selector_id_t);
