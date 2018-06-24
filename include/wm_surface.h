#ifndef __WM_SURFACE_H
#define __WM_SURFACE_H

#include <wayland-server.h>

#define WM_SURFACE_TYPE_X11 0
#define WM_SURFACE_TYPE_XDG 1
#define WM_SURFACE_TYPE_XDG_V6 2

struct wlr_surface;

typedef void (*wm_surface_render_handler)(struct wlr_surface *surface,
  int sx, int sy, void *data);

struct wm_surface {
  struct wm_server *server;
  struct wm_window *window;

  struct wlr_surface *surface;

  struct wl_listener commit;
  struct wl_listener map;
  struct wl_listener maximize;
  struct wl_listener move;
  struct wl_listener resize;
  struct wl_listener unmap;

  void (*render)(struct wm_surface* this,
    wm_surface_render_handler render_handler, void* data);

  void (*toplevel_set_size)(struct wm_surface* this, int width, int height);

  void (*toplevel_set_maximized)(struct wm_surface* this, bool maximized);

  void (*toplevel_constrained_set_size)(struct wm_surface* this,
    int width, int height);
};

void handle_move(struct wl_listener *listener, void *data);

void handle_resize(struct wl_listener *listener, void *data);

void handle_map(struct wl_listener *listener, void *data);

void handle_unmap(struct wl_listener *listener, void *data);

#endif
