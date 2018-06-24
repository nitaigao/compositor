#ifndef __WM_SURFACE_H
#define __WM_SURFACE_H

#include <wayland-server.h>

#define WM_SURFACE_TYPE_X11 0
#define WM_SURFACE_TYPE_XDG 1
#define WM_SURFACE_TYPE_XDG_V6 2

struct wm_surface {
  int type;

  struct wm_server *server;
  struct wm_window *window;

  struct wlr_surface *surface;

  struct wlr_xdg_surface *xdg_surface;
  struct wlr_xdg_surface_v6 *xdg_surface_v6;
  struct wlr_xwayland_surface *xwayland_surface;

  struct wl_listener commit;
  struct wl_listener map;
  struct wl_listener maximize;
  struct wl_listener move;
  struct wl_listener resize;
  struct wl_listener unmap;
};

struct wm_surface* wm_surface_xdg_v6_create(struct wlr_xdg_surface_v6* xdg_surface_v6,
  struct wm_server* server);

struct wm_surface* wm_surface_xdg_create(struct wlr_xdg_surface* xdg_surface,
  struct wm_server* server);

void handle_move(struct wl_listener *listener, void *data);

void handle_resize(struct wl_listener *listener, void *data);

void handle_map(struct wl_listener *listener, void *data);

void handle_unmap(struct wl_listener *listener, void *data);

#endif
