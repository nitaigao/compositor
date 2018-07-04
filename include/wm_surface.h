#ifndef __WM_SURFACE_H
#define __WM_SURFACE_H

#include <wayland-server.h>

struct wlr_surface;
struct timespec;
struct wm_seat;

typedef void (*wm_surface_render_handler)(struct wlr_surface *surface,
  int sx, int sy, void *data);

typedef void (*wm_surface_frame_done_handler)(struct wlr_surface *surface,
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
  struct wl_listener new_popup;
  struct wl_listener request_configure;

  void (*render)(struct wm_surface* this,
    wm_surface_render_handler render_handler, void* data);

  void (*frame_done)(struct wm_surface* this,
    wm_surface_frame_done_handler frame_donew_handler, struct timespec* now);

  void (*toplevel_set_size)(struct wm_surface* this, int width, int height);

  void (*toplevel_set_maximized)(struct wm_surface* this, bool maximized);

  void (*toplevel_constrained_set_size)(struct wm_surface* this,
    int width, int height);

  void (*toplevel_set_focused)(struct wm_surface* this,
    struct wm_seat* seat, bool focused);

  struct wlr_surface* (*wlr_surface_at)(struct wm_surface* this,
    double sx, double sy, double *sub_x, double *sub_y);

  struct wm_seat* (*locate_seat)(struct wm_surface* this);
};

void handle_move(struct wl_listener *listener, void *data);

void handle_resize(struct wl_listener *listener, void *data);

void handle_map(struct wl_listener *listener, void *data);

void handle_unmap(struct wl_listener *listener, void *data);

#endif
