#define _POSIX_C_SOURCE 200809L

#include "wm_surface.h"

#include <stdlib.h>
#include <string.h>
#include <wlr/backend.h>
#include <wlr/xwayland.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_wl_shell.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_screenshooter.h>
#include <wlr/types/wlr_gamma_control.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_output.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>

#include "wm_window.h"
#include "wm_server.h"
#include "wm_seat.h"
#include "wm_pointer.h"

void handle_map(struct wl_listener *listener, void *data) {
  (void)data;

  struct wm_surface *surface = wl_container_of(listener, surface, map);

  if (surface->type == WM_SURFACE_TYPE_X11) {
    surface->surface = surface->xwayland_surface->surface;
  }

  if (surface->type == WM_SURFACE_TYPE_XDG) {
    surface->surface = surface->xdg_surface->surface;
  }

  if (surface->type == WM_SURFACE_TYPE_XDG_V6) {
    surface->surface = surface->xdg_surface_v6->surface;
  }

  struct wm_window *window = calloc(1, sizeof(struct wm_window));
  window->x = 0;
  window->y = 0;
  window->width = surface->surface->current->width;
  window->height = surface->surface->current->height;
  window->surface = surface;

  surface->window = window;

  wl_list_insert(&surface->server->windows, &window->link);

  struct wm_seat *seat = wm_seat_find_or_create(window->surface->server, WM_DEFAULT_SEAT);

  wlr_seat_keyboard_notify_enter(
    seat->seat,
    surface->surface,
    NULL, 0, NULL
  );
}

void handle_unmap(struct wl_listener *listener, void *data) {
  (void)data;

  struct wm_surface *wm_surface = wl_container_of(listener, wm_surface, unmap);

  wl_list_remove(&wm_surface->window->link);
  wl_list_remove(&wm_surface->unmap.link);
  wl_list_remove(&wm_surface->map.link);

  int list_length = wl_list_length(&wm_surface->server->windows);

  if (list_length > 0) {
    struct wm_window *window;
    wl_list_for_each(window, &wm_surface->server->windows, link) {
      break;
    }

    struct wm_seat *seat = wm_seat_find_or_create(window->surface->server, WM_DEFAULT_SEAT);

    wlr_seat_keyboard_notify_enter(
      seat->seat,
      window->surface->surface,
      NULL, 0, NULL
    );
  }

  free(wm_surface->window);
  free(wm_surface);
}

void handle_move(struct wl_listener *listener, void *data) {
  struct wm_surface *surface = wl_container_of(listener, surface, move);
  struct wlr_wl_shell_surface_move_event *event = data;
  struct wm_seat *seat = wm_seat_find_or_create(surface->server, event->seat->seat->name);
  seat->pointer->mode = WM_POINTER_MODE_MOVE;
}

static void handle_resize(struct wl_listener *listener, void *data) {
  struct wm_surface *surface =
		wl_container_of(listener, surface, resize);

  struct wlr_wl_shell_surface_resize_event *e = data;

  struct wm_seat* seat = wm_seat_find_or_create(surface->server, e->seat->seat->name);

  if (seat->pointer) {
    wm_pointer_set_mode(seat->pointer, WM_POINTER_MODE_RESIZE);
  }
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

  return wm_surface;
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

  return wm_surface;
}
