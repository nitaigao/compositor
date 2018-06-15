#ifndef __WM_SURFACE_H
#define __WM_SURFACE_H

#define WM_SURFACE_TYPE_X11 0
#define WM_SURFACE_TYPE_XDG_V6 1

struct wm_surface {
  int type;
  double scale;

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
