#define _POSIX_C_SOURCE 200809L

#include "wm_shell_xwayland.h"

#include <stdlib.h>
#include <wlr/xwayland.h>

#include "wm_pointer.h"
#include "wm_seat.h"
#include "wm_server.h"
#include "wm_shell.h"
#include "wm_surface.h"
#include "wm_window.h"

static void handle_xwayland_commit(struct wl_listener *listener, void *data) {
  (void)data;

  struct wm_surface *surface = wl_container_of(listener, surface, commit);

  struct wlr_xwayland_surface* xwayland_surface =
    wlr_xwayland_surface_from_wlr_surface(surface->surface);

  if (!xwayland_surface->mapped) {
    return;
  }

  wm_window_commit_pending_movement(surface->window,
    xwayland_surface->width, xwayland_surface->height);
}

void handle_xwayland_map(struct wl_listener *listener, void *data) {
  struct wlr_xwayland_surface *xwayland_surface = data;
  struct wm_surface *surface = wl_container_of(listener, surface, map);

  surface->surface = xwayland_surface->surface;

  struct wm_window *window = calloc(1, sizeof(struct wm_window));
  window->x = xwayland_surface->x;
  window->y = xwayland_surface->y;
  window->name = xwayland_surface->title;
  window->width = xwayland_surface->width;
  window->height = xwayland_surface->height;
  window->surface = surface;
  window->pending_width = window->width;
  window->pending_height = window->height;
  window->pending_x = window->x;
  window->pending_y = window->y;
  window->xwindow = true;

  surface->window = window;

  surface->commit.notify = handle_xwayland_commit;
	wl_signal_add(&xwayland_surface->surface->events.commit, &surface->commit);

  struct wm_seat *seat = wm_seat_find_or_create(window->surface->server, WM_DEFAULT_SEAT);
  wm_server_add_window(surface->server, window, seat);
}

static void handle_xwayland_request_configure(struct wl_listener *listener, void *data) {
  struct wm_surface *surface = wl_container_of(listener, surface, request_configure);

  struct wlr_xwayland_surface_configure_event *event = data;

  if (!surface->surface) {
    return;
  }

  struct wlr_xwayland_surface* xwayland_surface =
    wlr_xwayland_surface_from_wlr_surface(surface->surface);

  printf("handle_xwayland_request_configure\n");

  wlr_xwayland_surface_configure(xwayland_surface, event->x, event->y,
    event->width, event->height);
}

static void handle_xwayland_maximize(struct wl_listener *listener, void *data) {
  (void)data;
  printf("handle_xwayland_maximize\n");
	struct wm_surface *surface = wl_container_of(listener, surface, maximize);
	struct wm_window *window = surface->window;
  if (!window) {
    printf("no window\n");
    return;
  }
  wm_window_maximize(window, !window->maximized);
}

void wm_surface_xwayland_render(struct wm_surface* this,
  wm_surface_render_handler render_surface, void *data) {
  (void)data;
  wlr_surface_for_each_surface(this->surface, render_surface, data);
}

void wm_surface_xwayland_frame_done(struct wm_surface* this,
  wm_surface_frame_done_handler send_frame_done,  struct timespec* now) {
  wlr_surface_for_each_surface(this->surface, send_frame_done, now);
}

void wm_surface_xwayland_toplevel_set_size(struct wm_surface* this,
  int width, int height) {
  printf("wm_surface_xwayland_toplevel_set_size\n");
  struct wlr_xwayland_surface* xwayland_surface =
    wlr_xwayland_surface_from_wlr_surface(this->surface);

  wlr_xwayland_surface_configure(xwayland_surface,
    this->window->x, this->window->y, width, height);
}

void wm_surface_xwayland_toplevel_set_maximized(struct wm_surface* this,
  bool maximized) {
  struct wlr_xwayland_surface* xwayland_surface =
    wlr_xwayland_surface_from_wlr_surface(this->surface);
  wlr_xwayland_surface_set_maximized(xwayland_surface, maximized);
}

void wm_surface_xwayland_constrained_set_size(struct wm_surface* this,
  int width, int height) {
  printf("wm_surface_xwayland_constrained_set_size\n");

  struct wlr_xwayland_surface* xwayland_surface =
    wlr_xwayland_surface_from_wlr_surface(this->surface);

  int constrained_height = height >= (int)xwayland_surface->size_hints->min_height
    ? height : (int)xwayland_surface->size_hints->min_height;

  int constrained_width = width >= (int)xwayland_surface->size_hints->min_width
    ? width : (int)xwayland_surface->size_hints->min_width;

  wlr_xwayland_surface_configure(xwayland_surface,
    this->window->x, this->window->y, constrained_width, constrained_height);
}

void wm_surface_xwayland_toplevel_set_focused(struct wm_surface* this,
  struct wm_seat* seat, bool focused) {
  struct wlr_xwayland_surface* xwayland_surface =
    wlr_xwayland_surface_from_wlr_surface(this->surface);

  if (focused) {
    wlr_seat_keyboard_notify_enter(
      seat->seat,
      xwayland_surface->surface,
      NULL, 0, NULL
    );
  }

  wlr_xwayland_surface_activate(xwayland_surface, focused);
}

struct wlr_surface* wm_surface_xwayland_wlr_surface_at(struct wm_surface* this,
		double sx, double sy, double *sub_x, double *sub_y) {
  struct wlr_surface *surface = wlr_surface_surface_at(
    this->surface, sx, sy, sub_x, sub_y);
  return surface;
}

bool wm_surface_xwayland_under_point(struct wm_surface* this,
  int sx, int sy) {
  double sub_x, sub_y;
  struct wlr_surface *surface = wlr_surface_surface_at(
    this->surface, sx, sy, &sub_x, &sub_y);
  bool is_under_point = this->surface == surface;
  return is_under_point;
}

struct wm_seat* wm_surface_xwayland_locate_seat(struct wm_surface* this) {
  struct wm_seat* default_seat = wm_seat_find_or_create(this->server,
    WM_DEFAULT_SEAT);
  return default_seat;
}

static void handle_xwayland_resize(struct wl_listener *listener, void *data) {
  struct wlr_xwayland_resize_event *e = data;
	struct wm_surface *surface = wl_container_of(listener, surface, resize);
  wm_surface_begin_resize(surface, e->edges);
}

struct wm_surface* wm_surface_xwayland_create(struct wlr_xwayland_surface* xwayland_surface,
  struct wm_server* server) {

  struct wm_surface *wm_surface = calloc(1, sizeof(struct wm_surface));
  wm_surface->server = server;
  wm_surface->render = wm_surface_xwayland_render;
  wm_surface->frame_done = wm_surface_xwayland_frame_done;
  wm_surface->toplevel_set_size = wm_surface_xwayland_toplevel_set_size;
  wm_surface->toplevel_set_maximized = wm_surface_xwayland_toplevel_set_maximized;
  wm_surface->toplevel_constrained_set_size = wm_surface_xwayland_constrained_set_size;
  wm_surface->toplevel_set_focused = wm_surface_xwayland_toplevel_set_focused;
  wm_surface->under_point = wm_surface_xwayland_under_point;
  wm_surface->wlr_surface_at = wm_surface_xwayland_wlr_surface_at;
  wm_surface->locate_seat = wm_surface_xwayland_locate_seat;

  wm_surface->map.notify = handle_xwayland_map;
  wl_signal_add(&xwayland_surface->events.map, &wm_surface->map);

  wm_surface->unmap.notify = handle_unmap;
  wl_signal_add(&xwayland_surface->events.unmap, &wm_surface->unmap);

  wm_surface->move.notify = handle_request_move;
  wl_signal_add(&xwayland_surface->events.request_move,
    &wm_surface->move);

  wm_surface->resize.notify = handle_xwayland_resize;
	wl_signal_add(&xwayland_surface->events.request_resize,
    &wm_surface->resize);

  wm_surface->maximize.notify = handle_xwayland_maximize;
	wl_signal_add(&xwayland_surface->events.request_maximize,
    &wm_surface->maximize);

  // wm_surface->minimize.notify = handle_minimize;
	// wl_signal_add(&xwayland_surface->events.request_minimize,
  //   &wm_surface->minimize);

  wm_surface->request_configure.notify = handle_xwayland_request_configure;
	wl_signal_add(&xwayland_surface->events.request_configure,
    &wm_surface->request_configure);

  return wm_surface;
}

void handle_xwayland_shell_surface(struct wl_listener *listener, void *data) {
  struct wlr_xwayland_surface *xwayland_surface = data;
  struct wm_shell *shell = wl_container_of(listener, shell, shell_surface);

  printf("New XWAYLAND Surface: Title=%s\n", xwayland_surface->title);
  wlr_xwayland_surface_ping(xwayland_surface);
  wm_surface_xwayland_create(xwayland_surface, shell->server);
}

struct wm_shell* wm_shell_xwayland_create(struct wm_server *server) {
  struct wm_shell* container = calloc(1, sizeof(struct wm_shell));
  struct wlr_xwayland* shell = wlr_xwayland_create(server->wl_display,
    server->compositor, true);
  wl_signal_add(&shell->events.new_surface, &container->shell_surface);
  container->shell_surface.notify = handle_xwayland_shell_surface;
  container->shell = shell;
  container->server = server;
  return container;
}
