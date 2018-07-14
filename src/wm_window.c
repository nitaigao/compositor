#include "wm_window.h"

#include <wlr/xwayland.h>
#include <wlr/util/edges.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_cursor.h>

#include "wm_server.h"
#include "wm_surface.h"
#include "wm_pointer.h"

struct wlr_box wm_window_geometry(struct wm_window* window) {
  struct wlr_box geometry = {
    .x = window->x,
    .y = window->y,
    .width = window->width,
    .height = window->height
  };
  return geometry;
}

bool wm_window_intersects_point(struct wm_window* window, int x, int y) {
  struct wlr_box geometry = wm_window_geometry(window);
  bool contains_point = wlr_box_contains_point(&geometry, x, y);
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

    if (pointer->resize_edge & WLR_EDGE_TOP) {
      y = pointer->window_y + dy;
      height -= dy;
      window->update_y = true;
    } else if (pointer->resize_edge & WLR_EDGE_BOTTOM) {
      height += dy;
    }

    if (pointer->resize_edge & WLR_EDGE_LEFT) {
      x = pointer->window_x + dx;
      width -= dx;
      window->update_x = true;
    } else if (pointer->resize_edge & WLR_EDGE_RIGHT) {
      width += dx;
    }

    window->pending_x = x;
    window->pending_width = width;

    window->pending_y = y;
    window->pending_height = height;

    window->surface->toplevel_constrained_set_size(
      window->surface, width, height);
}

void wm_window_move(struct wm_window* window, int x, int y) {
  window->x = x;
  window->y = y;
}

void wm_window_minimize(struct wm_window* window, bool minimized) {
  window->minimized = minimized;
}

void wm_window_maximize(struct wm_window* window, bool maximized) {
  window->maximized = maximized;

  if (maximized) {
    wm_window_save_geography(window);
    struct wlr_output* output = wm_window_find_output(window);

    struct wlr_box *fullscreen_box =
      wlr_output_layout_get_box(window->surface->server->layout, output);

    wm_window_move(window, fullscreen_box->x, fullscreen_box->y);

    window->surface->toplevel_set_maximized(window->surface, maximized);

    window->surface->toplevel_set_size(window->surface,
      fullscreen_box->width, fullscreen_box->height);
  } else {
    wm_window_move(window, window->saved_x, window->saved_y);
    window->surface->toplevel_set_size(window->surface,
      window->saved_width, window->saved_height);

    window->surface->toplevel_set_maximized(window->surface, maximized);
  }
}

void wm_window_save_geography(struct wm_window* window) {
  window->saved_x = window->x;
  window->saved_y = window->y;
  window->saved_width = window->width;
  window->saved_height = window->height;

  printf("Saved geometry x: %d y: %d width: %d height: %d\n",
    window->saved_x, window->saved_y, window->saved_width, window->saved_height);
}

struct wlr_output* wm_window_find_output(struct wm_window* window) {
  struct wlr_box geometry = wm_window_geometry(window);

  double output_x, output_y;
  wlr_output_layout_closest_point(window->surface->server->layout, NULL,
    window->x + (double)geometry.width / 2,
    window->y + (double)geometry.height / 2,
    &output_x, &output_y
  );

  struct wlr_output* output = wlr_output_layout_output_at(
    window->surface->server->layout, output_x, output_y);

  return output;
}

bool wm_window_under_point(struct wm_window* window, int x, int y) {
  int sx = x - window->x;
  int sy = y - window->y;
  bool is_under_point = window->surface->under_point(window->surface, sx, sy);
  return is_under_point;
}
