#include "wm_window.h"

#include <wlr/xwayland.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_cursor.h>

#include "wm_surface.h"
#include "wm_pointer.h"

void wm_window_focus(struct wm_window* window) {
  if (window->surface->type == WM_SURFACE_TYPE_X11) {
    wlr_xwayland_surface_activate(window->surface->xwayland_surface, true);
  }

  if (window->surface->type == WM_SURFACE_TYPE_XDG_V6) {
    wlr_xdg_toplevel_v6_set_activated(window->surface->xdg_surface_v6, true);
  }
}

bool wm_window_intersects_point(struct wm_window* window, int x, int y) {
  struct wlr_box box = {
    .x = window->x,
    .y = window->y,
    .width = window->width,
    .height = window->height
  };
  bool contains_point = wlr_box_contains_point(&box, x, y);
  return contains_point;
}

void wm_window_commit_pending_movement(struct wm_window* window, int width, int height) {
  window->width = width;
  window->height = height;

  if (window->update_y) {
    window->y = window->pending_y +
      window->pending_height - height;
  }

  if (window->update_x) {
    window->x = window->pending_x +
      window->pending_width - width;
  }
}


void wm_window_resize(struct wm_window* window, struct wm_pointer* pointer) {
    int dx = pointer->cursor->x - pointer->offset_x;
		int dy = pointer->cursor->y - pointer->offset_y;

    int x = window->x;
    int y = window->y;

    int width = pointer->window_width;
    int height = pointer->window_height;

    if (pointer->resize_edge == WL_SHELL_SURFACE_RESIZE_TOP) {
      y = pointer->window_y + dy;
      height -= dy;
      window->update_y = true;
    }

    if (pointer->resize_edge == WL_SHELL_SURFACE_RESIZE_BOTTOM) {
      height += dy;
    }

    if (pointer->resize_edge == WL_SHELL_SURFACE_RESIZE_LEFT) {
      x = pointer->window_x + dx;
      width -= dx;
      window->update_x = true;
    }

    if (pointer->resize_edge == WL_SHELL_SURFACE_RESIZE_RIGHT) {
      width += dx;
    }

    if (pointer->resize_edge == WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT) {
      x = pointer->window_x + dx;
      width -= dx;
      window->update_x = true;
      height += dy;
    }

    if (pointer->resize_edge == WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT) {
      height += dy;
      width += dx;
    }

    if (pointer->resize_edge == WL_SHELL_SURFACE_RESIZE_TOP_LEFT) {
      y = pointer->window_y + dy;
      height -= dy;
      window->update_y = true;
      x = pointer->window_x + dx;
      width -= dx;
      window->update_x = true;
    }

    if (pointer->resize_edge == WL_SHELL_SURFACE_RESIZE_TOP_RIGHT) {
      y = pointer->window_y + dy;
      height -= dy;
      window->update_y = true;
      width += dx;
    }

    window->pending_x = x;
    window->pending_width = width;

    window->pending_y = y;
    window->pending_height = height;

    struct wlr_xdg_toplevel_state state =
      window->surface->xdg_surface->toplevel->current;

    int constrained_height = height >= (int)state.min_height ? height : (int)state.min_height;
    int constrained_width = width >= (int)state.min_width ? width : (int)state.min_width;

    wlr_xdg_toplevel_set_size(window->surface->xdg_surface, constrained_width, constrained_height);
}

void wm_window_move(struct wm_window* window, struct wm_pointer* pointer) {
  int dx = pointer->cursor->x - pointer->offset_x;
  int dy = pointer->cursor->y - pointer->offset_y;

  window->x = pointer->window_x + dx;
  window->y = pointer->window_y + dy;
}
