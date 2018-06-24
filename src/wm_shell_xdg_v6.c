#define _POSIX_C_SOURCE 200809L

#include "wm_shell_xdg_v6.h"

#include <stdlib.h>
#include <wlr/types/wlr_xdg_shell_v6.h>

#include "wm_pointer.h"
#include "wm_seat.h"
#include "wm_server.h"
#include "wm_shell.h"
#include "wm_surface.h"
#include "wm_window.h"

static void handle_map_v6(struct wl_listener *listener, void *data) {
  (void)data;

  struct wm_surface *surface = wl_container_of(listener, surface, map);

  struct wlr_xdg_surface_v6* xdg_surface_v6 =
    wlr_xdg_surface_v6_from_wlr_surface(surface->surface);

  surface->surface = xdg_surface_v6->surface;

  struct wm_window *window = calloc(1, sizeof(struct wm_window));
  window->x = 50;
  window->y = 50;
  window->width = surface->surface->current->width;
  window->height = surface->surface->current->height;
  window->surface = surface;
  window->pending_height = window->height;
  window->pending_y = window->y;

  surface->window = window;

  wl_list_insert(&surface->server->windows, &window->link);

  struct wm_seat *seat = wm_seat_find_or_create(window->surface->server, WM_DEFAULT_SEAT);

  wlr_seat_keyboard_notify_enter(
    seat->seat,
    surface->surface,
    NULL, 0, NULL
  );
}

static void handle_xdg_v6_commit(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_surface *surface =
		wl_container_of(listener, surface, commit);

  struct wlr_xdg_surface_v6* xdg_surface_v6 = wlr_xdg_surface_v6_from_wlr_surface(surface->surface);

  if (!xdg_surface_v6->mapped) {
    return;
  }

  struct wlr_box geometry;
	wlr_xdg_surface_v6_get_geometry(xdg_surface_v6, &geometry);
  wm_window_commit_pending_movement(surface->window, geometry.width, geometry.height);
}

static void handle_xdg_v6_maximize(struct wl_listener *listener, void *data) {
  (void)data;
	struct wm_surface *surface = wl_container_of(listener, surface, maximize);
	struct wm_window *window = surface->window;
  struct wlr_xdg_surface_v6* xdg_surface_v6 = wlr_xdg_surface_v6_from_wlr_surface(surface->surface);

	if (xdg_surface_v6->role != WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		return;
  }
  wm_window_maximize(window, !window->maximized);
}

void wm_surface_xdg_v6_render(struct wm_surface* this,
  wm_surface_render_handler render_surface, void *data) {
  struct wlr_xdg_surface_v6* xdg_surface_v6 =
    wlr_xdg_surface_v6_from_wlr_surface(this->surface);
  wlr_xdg_surface_v6_for_each_surface(xdg_surface_v6,
    render_surface, data);
}

void wm_surface_xdg_v6_toplevel_set_size(struct wm_surface* this,
  int width, int height) {
  struct wlr_xdg_surface_v6* xdg_surface_v6 =
    wlr_xdg_surface_v6_from_wlr_surface(this->surface);
  wlr_xdg_toplevel_v6_set_size(xdg_surface_v6, width, height);
}

void wm_surface_xdg_toplevel_v6_set_maximized(struct wm_surface* this, bool maximized) {
  struct wlr_xdg_surface_v6* xdg_surface_v6 =
    wlr_xdg_surface_v6_from_wlr_surface(this->surface);
  wlr_xdg_toplevel_v6_set_maximized(xdg_surface_v6, maximized);
}

void wm_surface_xdg_v6_constrained_set_size(struct wm_surface* this,
  int width, int height) {
  struct wlr_xdg_surface_v6* xdg_surface_v6 =
    wlr_xdg_surface_v6_from_wlr_surface(this->surface);

  struct wlr_xdg_toplevel_v6_state state = xdg_surface_v6->toplevel->current;

  int constrained_height = height >= (int)state.min_height
    ? height : (int)state.min_height;

  int constrained_width = width >= (int)state.min_width
    ? width : (int)state.min_width;

  wlr_xdg_toplevel_v6_set_size(xdg_surface_v6,
    constrained_width, constrained_height);
}

struct wm_surface* wm_surface_xdg_v6_create(struct wlr_xdg_surface_v6* xdg_surface_v6, struct wm_server* server) {
  struct wm_surface *wm_surface = calloc(1, sizeof(struct wm_surface));
  wm_surface->server = server;
  wm_surface->surface = xdg_surface_v6->surface;
  wm_surface->render = wm_surface_xdg_v6_render;
  wm_surface->toplevel_set_size = wm_surface_xdg_v6_toplevel_set_size;
  wm_surface->toplevel_set_maximized = wm_surface_xdg_toplevel_v6_set_maximized;
  wm_surface->toplevel_constrained_set_size = wm_surface_xdg_v6_constrained_set_size;

  if (xdg_surface_v6->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
    wlr_xdg_toplevel_v6_set_activated(xdg_surface_v6, true);
  }

  wm_surface->map.notify = handle_map_v6;
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

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data) {
  struct wlr_xdg_surface_v6 *xdg_surface_v6 = data;
  struct wm_shell *shell = wl_container_of(listener, shell, shell_surface);

  if (xdg_surface_v6->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
    printf("Popup requested, dropping\n");
    return;
  }

  printf("New XDG V6 Surface: Title=%s\n", xdg_surface_v6->toplevel->title);
  wlr_xdg_surface_v6_ping(xdg_surface_v6);
  wm_surface_xdg_v6_create(xdg_surface_v6, shell->server);
}

struct wm_shell* wm_shell_xdg_v6_create(struct wm_server *server) {
  struct wm_shell* container = calloc(1, sizeof(struct wm_shell));
  struct wlr_xdg_shell_v6* shell = wlr_xdg_shell_v6_create(server->wl_display);
  wl_signal_add(&shell->events.new_surface, &container->shell_surface);
  container->shell_surface.notify = handle_xdg_shell_v6_surface;
  container->shell = shell;
  container->server = server;
  return container;
}
