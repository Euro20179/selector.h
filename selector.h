/*

selector.h is a library to create a selection screen with a customizable preview.

Usage:
call selector_new2 with a struct selector_action_handlers, and call selector_del2 to clean up

selector_get_by_id will get an item by selector_id_t id from the provided items array.

*/
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "./c-stdlib/array.h"
#include "./c-stdlib/string.h"

typedef size_t selector_id_t;

struct selector_preview_info {
    selector_id_t id;
    size_t width;
};

struct selector_action_handlers {
    void(*on_hover)(selector_id_t);
    ///function that will generate the preview text
    ///all text goes into preview_gen
    ///caller must call string_new2 on the string to return
    ///ownership is then released on return
    string*(*preview_gen)(struct selector_preview_info);
};

typedef struct {
    void(*on_hover)(selector_id_t);
    string*(*preview_gen)(struct selector_preview_info);
    array* items;

    size_t height;
    size_t width;
    selector_id_t selected;

    //state
    bool searching;
    string* current_search;
    array* visible_items;

    size_t* item_ids;
} selector;

///allocates a new selector
///caller MUST call selector_del2 to free the selector
///array MUST be a const char** array
selector* selector_new2(struct selector_action_handlers, array*);

void selector_del2(selector*);

///open the selection screen for the user to select an item
///puts the exit code into the 2nd out parameter
///exit codes:
/// 2: ctrl-c or q
/// 1: enter
///RETURNS: the id of the selected item
selector_id_t selector_select(selector*, int*);

///gets an item from the provided items by id
const char* selector_get_by_id(selector*, selector_id_t);
