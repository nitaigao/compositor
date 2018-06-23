#include "wm_pointer.h"

#include <wlr/types/wlr_xcursor_manager.h>

#include "wm_server.h"

#define DEFAULT_CURSOR "left_ptr"

void wm_pointer_set_mode(struct wm_pointer* pointer, int mode) {
  pointer->mode = mode;
}

void wm_pointer_set_default_cursor(struct wm_pointer* pointer) {
  wlr_xcursor_manager_set_cursor_image(pointer->server->xcursor_manager,
			DEFAULT_CURSOR, pointer->cursor);
}

void wm_pointer_set_resize_edge(struct wm_pointer* pointer, int resize_edge) {
  pointer->resize_edge = resize_edge;
}
