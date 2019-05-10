#define _POSIX_C_SOURCE 200809L

#include "wm_surface.h"

#include <stdlib.h>
#include <string.h>
#include <wlr/backend.h>
#include <wlr/xwayland.h>
#include <wlr/backend/session.h>
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
#include <wlr/types/wlr_xdg_output_v1.h>
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

  printf("handle_unmap\n");

  struct wm_surface *wm_surface = wl_container_of(listener, wm_surface, unmap);

  wl_list_remove(&wm_surface->unmap.link);
  wl_list_remove(&wm_surface->map.link);

  int list_length = wl_list_length(&wm_surface->server->windows);

  if (list_length > 0) {
    struct wm_window *window = wl_list_first(&wm_surface->server->windows,
      window, link);

    struct wm_seat *seat = wm_seat_find_or_create(window->surface->server,
      WM_DEFAULT_SEAT);

    wm_server_switch_window(window->surface->server);
    wm_server_commit_window_switch(window->surface->server, seat);
  }

  wm_server_remove_window(wm_surface->window);

  free(wm_surface->window);
  free(wm_surface);
}

void handle_request_move(struct wl_listener *listener, void *data) {
  struct wm_surface *surface = wl_container_of(listener, surface, move);
  struct wlr_wl_shell_surface_move_event *event = data;
  (void)event;
  printf("handle_request_move\n");

  struct wm_seat *seat = surface->locate_seat(surface);

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

void handle_minimize(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_surface *surface = wl_container_of(listener, surface, minimize);
  wm_window_minimize(surface->window, true);
}

void wm_surface_begin_resize(struct wm_surface* surface, uint32_t edges) {
  struct wm_seat* seat = surface->locate_seat(surface);

  wm_pointer_set_resize_edge(seat->pointer, edges);

  if (seat->pointer) {
    seat->pointer->offset_x = seat->pointer->cursor->x;
    seat->pointer->offset_y = seat->pointer->cursor->y;
    seat->pointer->window_x = surface->window->x;
    seat->pointer->window_y = surface->window->y;
    seat->pointer->window_width = surface->window->width;
    seat->pointer->window_height = surface->window->height;
    wm_pointer_set_mode(seat->pointer, WM_POINTER_MODE_RESIZE);
  }

  wlr_seat_pointer_clear_focus(seat->seat);
}
