#ifndef __WM_SURFACE_H
#define __WM_SURFACE_H

struct wm_surface {
  struct wm_server *server;
  struct wm_window *window;

  struct wlr_xdg_surface_v6 *xdg_surface_v6;
  struct wlr_xwayland_surface *xwayland_surface;
  struct wlr_surface *surface;

  struct wl_listener commit;
  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener move;
};

#endif
