#define _POSIX_C_SOURCE 200809L

#include "wm_surface.h"

#include <stdlib.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "wm_pointer.h"
#include "wm_seat.h"
#include "wm_server.h"
#include "wm_surface.h"
#include "wm_window.h"

static void handle_xdg_commit(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_surface *surface =
		wl_container_of(listener, surface, commit);

  if (!surface->xdg_surface->mapped) {
    return;
  }

  struct wlr_box geometry;
	wlr_xdg_surface_get_geometry(surface->xdg_surface, &geometry);
  wm_window_commit_pending_movement(surface->window, geometry.width, geometry.height);
}

static void handle_xdg_maximize(struct wl_listener *listener, void *data) {
  (void)data;
	struct wm_surface *surface = wl_container_of(listener, surface, maximize);
	struct wm_window *window = surface->window;
	if (surface->xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
  }
  wm_window_maximize(window, !window->maximized);
}

struct wm_surface* wm_surface_xdg_create(struct wlr_xdg_surface* xdg_surface,
  struct wm_server* server) {

  struct wm_surface *wm_surface = calloc(1, sizeof(struct wm_surface));
  wm_surface->server = server;
  wm_surface->xdg_surface = xdg_surface;
  wm_surface->type = WM_SURFACE_TYPE_XDG;

  if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
    wlr_xdg_toplevel_set_activated(xdg_surface, true);
  }

  wm_surface->map.notify = handle_map;
  wl_signal_add(&xdg_surface->events.map, &wm_surface->map);

  wm_surface->unmap.notify = handle_unmap;
  wl_signal_add(&xdg_surface->events.unmap, &wm_surface->unmap);

  wm_surface->move.notify = handle_move;
  wl_signal_add(&xdg_surface->toplevel->events.request_move, &wm_surface->move);

  wm_surface->resize.notify = handle_resize;
	wl_signal_add(&xdg_surface->toplevel->events.request_resize, &wm_surface->resize);

  wm_surface->maximize.notify = handle_xdg_maximize;
	wl_signal_add(&xdg_surface->toplevel->events.request_maximize, &wm_surface->maximize);

  wm_surface->commit.notify = handle_xdg_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &wm_surface->commit);

  return wm_surface;
}
