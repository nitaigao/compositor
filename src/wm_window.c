#include "wm_window.h"

#include <wlr/xwayland.h>
#include <wlr/types/wlr_xdg_shell_v6.h>

#include "wm_surface.h"

void window_focus(struct wm_window* window) {
  if (window->surface->type == WM_SURFACE_TYPE_X11) {
    wlr_xwayland_surface_activate(window->surface->xwayland_surface, true);
  }

  if (window->surface->type == WM_SURFACE_TYPE_XDG_V6) {
    wlr_xdg_toplevel_v6_set_activated(window->surface->xdg_surface_v6, true);
  }
}
