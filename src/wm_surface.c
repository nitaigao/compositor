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

  if (seat->pointer && seat->pointer->mode != WM_POINTER_MODE_MOVE) {
    seat->pointer->offset_x = seat->pointer->cursor->x;
    seat->pointer->offset_y = seat->pointer->cursor->y;
    seat->pointer->window_x = surface->window->x;
    seat->pointer->window_y = surface->window->y;
    seat->pointer->window_width = surface->window->width;
    seat->pointer->window_height = surface->window->height;
    seat->pointer->mode = WM_POINTER_MODE_MOVE;
  }
}

void handle_resize(struct wl_listener *listener, void *data) {
  struct wm_surface *surface =
		wl_container_of(listener, surface, resize);

  struct wlr_wl_shell_surface_resize_event *e = data;

  struct wm_seat* seat = wm_seat_find_or_create(surface->server, e->seat->seat->name);

  if (seat->pointer && seat->pointer->mode != WM_POINTER_MODE_RESIZE) {
    seat->pointer->offset_x = seat->pointer->cursor->x;
    seat->pointer->offset_y = seat->pointer->cursor->y;
    seat->pointer->window_x = surface->window->x;
    seat->pointer->window_y = surface->window->y;
    seat->pointer->window_width = surface->window->width;
    seat->pointer->window_height = surface->window->height;

    wm_pointer_set_mode(seat->pointer, WM_POINTER_MODE_RESIZE);
    wm_pointer_set_resize_edge(seat->pointer, e->edges);
  }
}
