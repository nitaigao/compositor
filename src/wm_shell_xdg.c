#define _POSIX_C_SOURCE 200809L

#include "wm_shell_xdg.h"

#include <stdlib.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "wm_pointer.h"
#include "wm_seat.h"
#include "wm_server.h"
#include "wm_shell.h"
#include "wm_surface.h"
#include "wm_window.h"

void handle_xdg_map(struct wl_listener *listener, void *data) {
  (void)data;

  struct wm_surface *surface = wl_container_of(listener, surface, map);
  struct wlr_xdg_surface* xdg_surface =
    wlr_xdg_surface_from_wlr_surface(surface->surface);
  surface->surface = xdg_surface->surface;

  struct wm_window *window = calloc(1, sizeof(struct wm_window));
  window->x = 50;
  window->y = 50;
  window->name = xdg_surface->toplevel->title;
  window->width = surface->surface->current->width;
  window->height = surface->surface->current->height;
  window->surface = surface;
  window->pending_height = window->height;
  window->pending_y = window->y;

  surface->window = window;

  struct wm_seat *seat = wm_seat_find_or_create(window->surface->server, WM_DEFAULT_SEAT);
  wm_server_add_window(surface->server, window, seat);
}

static void handle_xdg_commit(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_surface *surface = wl_container_of(listener, surface, commit);

  struct wlr_xdg_surface* xdg_surface =
    wlr_xdg_surface_from_wlr_surface(surface->surface);

  if (!xdg_surface->mapped) {
    return;
  }

  struct wlr_box geometry;
	wlr_xdg_surface_get_geometry(xdg_surface, &geometry);
  wm_window_commit_pending_movement(surface->window, geometry.width, geometry.height);
}

static void handle_xdg_maximize(struct wl_listener *listener, void *data) {
  (void)data;
	struct wm_surface *surface = wl_container_of(listener, surface, maximize);
	struct wm_window *window = surface->window;

  struct wlr_xdg_surface* xdg_surface = wlr_xdg_surface_from_wlr_surface(
    surface->surface);

	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
  }
  wm_window_maximize(window, !window->maximized);
}

void wm_surface_xdg_render(struct wm_surface* this,
  wm_surface_render_handler render_surface, void *data) {
  struct wlr_xdg_surface* xdg_surface =
    wlr_xdg_surface_from_wlr_surface(this->surface);
  wlr_xdg_surface_for_each_surface(xdg_surface, render_surface, data);
}

void wm_surface_xdg_frame_done(struct wm_surface* this,
  wm_surface_frame_done_handler send_frame_done,  struct timespec* now) {
  struct wlr_xdg_popup *popup;

  wlr_surface_for_each_surface(this->surface, send_frame_done, now);

  struct wlr_xdg_surface* xdg_surface =
    wlr_xdg_surface_from_wlr_surface(this->surface);

  wl_list_for_each_reverse(popup, &xdg_surface->popups, link) {
    wlr_surface_for_each_surface(popup->base->surface, send_frame_done, now);
  }
}

void wm_surface_xdg_toplevel_set_size(struct wm_surface* this,
  int width, int height) {
  struct wlr_xdg_surface* xdg_surface =
    wlr_xdg_surface_from_wlr_surface(this->surface);
  wlr_xdg_toplevel_set_size(xdg_surface, width, height);
}

void wm_surface_xdg_toplevel_set_maximized(struct wm_surface* this,
  bool maximized) {
  struct wlr_xdg_surface* xdg_surface =
    wlr_xdg_surface_from_wlr_surface(this->surface);
  wlr_xdg_toplevel_set_maximized(xdg_surface, maximized);
}

void wm_surface_xdg_constrained_set_size(struct wm_surface* this,
  int width, int height) {
  struct wlr_xdg_surface* xdg_surface =
    wlr_xdg_surface_from_wlr_surface(this->surface);

  struct wlr_xdg_toplevel_state state = xdg_surface->toplevel->current;

  int constrained_height = height >= (int)state.min_height
    ? height : (int)state.min_height;

  int constrained_width = width >= (int)state.min_width
    ? width : (int)state.min_width;

  wlr_xdg_toplevel_set_size(xdg_surface,
    constrained_width, constrained_height);
}

void wm_surface_xdg_toplevel_set_focused(struct wm_surface* this,
  struct wm_seat* seat, bool focused) {
  struct wlr_xdg_surface* xdg_surface =
    wlr_xdg_surface_from_wlr_surface(this->surface);

  if (focused) {
    wlr_seat_keyboard_notify_enter(
      seat->seat,
      xdg_surface->surface,
      NULL, 0, NULL
    );
  }

  if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
    wlr_xdg_toplevel_set_activated(xdg_surface, focused);
  }
}

struct wlr_surface* wm_surface_xdg_wlr_surface_at(struct wm_surface* this,
		double sx, double sy, double *sub_x, double *sub_y) {
  struct wlr_xdg_surface* xdg_surface = wlr_xdg_surface_from_wlr_surface(
    this->surface);
  struct wlr_surface *surface = wlr_xdg_surface_surface_at(
    xdg_surface, sx, sy, sub_x, sub_y);
  return surface;
}

struct wm_surface* wm_surface_xdg_create(struct wlr_xdg_surface* xdg_surface,
  struct wm_server* server) {

  struct wm_surface *wm_surface = calloc(1, sizeof(struct wm_surface));
  wm_surface->server = server;
  wm_surface->surface = xdg_surface->surface;
  wm_surface->render = wm_surface_xdg_render;
  wm_surface->frame_done = wm_surface_xdg_frame_done;
  wm_surface->toplevel_set_size = wm_surface_xdg_toplevel_set_size;
  wm_surface->toplevel_set_maximized = wm_surface_xdg_toplevel_set_maximized;
  wm_surface->toplevel_constrained_set_size = wm_surface_xdg_constrained_set_size;
  wm_surface->toplevel_set_focused = wm_surface_xdg_toplevel_set_focused;
  wm_surface->wlr_surface_at = wm_surface_xdg_wlr_surface_at;

  if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
    wlr_xdg_toplevel_set_activated(xdg_surface, true);
  }

  wm_surface->map.notify = handle_xdg_map;
  wl_signal_add(&xdg_surface->events.map, &wm_surface->map);

  wm_surface->unmap.notify = handle_unmap;
  wl_signal_add(&xdg_surface->events.unmap, &wm_surface->unmap);

  wm_surface->move.notify = handle_move;
  wl_signal_add(&xdg_surface->toplevel->events.request_move,
    &wm_surface->move);

  wm_surface->resize.notify = handle_resize;
	wl_signal_add(&xdg_surface->toplevel->events.request_resize,
    &wm_surface->resize);

  wm_surface->maximize.notify = handle_xdg_maximize;
	wl_signal_add(&xdg_surface->toplevel->events.request_maximize,
    &wm_surface->maximize);

  wm_surface->commit.notify = handle_xdg_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &wm_surface->commit);

  return wm_surface;
}

void handle_xdg_shell_surface(struct wl_listener *listener, void *data) {
  struct wlr_xdg_surface *xdg_surface = data;
  struct wm_shell *shell = wl_container_of(listener, shell, shell_surface);

  if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
    printf("Popup requested, dropping\n");
    return;
  }

  printf("New XDG Surface: Title=%s\n", xdg_surface->toplevel->title);
  wlr_xdg_surface_ping(xdg_surface);
  wm_surface_xdg_create(xdg_surface, shell->server);
}

struct wm_shell* wm_shell_xdg_create(struct wm_server *server) {
  struct wm_shell* container = calloc(1, sizeof(struct wm_shell));
  struct wlr_xdg_shell* shell = wlr_xdg_shell_create(server->wl_display);
  wl_signal_add(&shell->events.new_surface, &container->shell_surface);
  container->shell_surface.notify = handle_xdg_shell_surface;
  container->shell = shell;
  container->server = server;
  return container;
}
