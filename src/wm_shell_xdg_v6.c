#define _POSIX_C_SOURCE 200809L

#include "wm_surface.h"

#include <stdlib.h>
#include <wlr/types/wlr_xdg_shell_v6.h>

#include "wm_pointer.h"
#include "wm_seat.h"
#include "wm_server.h"
#include "wm_surface.h"
#include "wm_window.h"

static void handle_xdg_v6_commit(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_surface *surface =
		wl_container_of(listener, surface, commit);

  if (!surface->xdg_surface_v6->mapped) {
    return;
  }

  struct wlr_box geometry;
	wlr_xdg_surface_v6_get_geometry(surface->xdg_surface_v6, &geometry);
  wm_window_commit_pending_movement(surface->window, geometry.width, geometry.height);
}

static void handle_xdg_v6_maximize(struct wl_listener *listener, void *data) {
  (void)data;
	struct wm_surface *surface = wl_container_of(listener, surface, maximize);
	struct wm_window *window = surface->window;
	if (surface->xdg_surface_v6->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		return;
  }
  wm_window_maximize(window, !window->maximized);
}

struct wm_surface* wm_surface_xdg_v6_create(struct wlr_xdg_surface_v6* xdg_surface_v6, struct wm_server* server) {
  struct wm_surface *wm_surface = calloc(1, sizeof(struct wm_surface));
  wm_surface->server = server;
  wm_surface->xdg_surface_v6 = xdg_surface_v6;
  wm_surface->type = WM_SURFACE_TYPE_XDG_V6;

  if (xdg_surface_v6->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
    wlr_xdg_toplevel_v6_set_activated(xdg_surface_v6, true);
  }

  wm_surface->map.notify = handle_map;
  wl_signal_add(&xdg_surface_v6->events.map, &wm_surface->map);

  wm_surface->unmap.notify = handle_unmap;
  wl_signal_add(&xdg_surface_v6->events.unmap, &wm_surface->unmap);

  wm_surface->move.notify = handle_move;
  wl_signal_add(&xdg_surface_v6->toplevel->events.request_move, &wm_surface->move);

  wm_surface->resize.notify = handle_resize;
	wl_signal_add(&xdg_surface_v6->toplevel->events.request_resize, &wm_surface->resize);

  wm_surface->maximize.notify = handle_xdg_v6_maximize;
	wl_signal_add(&xdg_surface_v6->toplevel->events.request_maximize, &wm_surface->maximize);

  wm_surface->commit.notify = handle_xdg_v6_commit;
	wl_signal_add(&xdg_surface_v6->surface->events.commit, &wm_surface->commit);

  return wm_surface;
}
