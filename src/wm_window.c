#include "wm_window.h"

#include <wlr/xwayland.h>
#include <wlr/types/wlr_xdg_shell_v6.h>

#include "wm_surface.h"

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
