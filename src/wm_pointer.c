#include "wm_pointer.h"

#include <stdlib.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>


#include "wm_seat.h"
#include "wm_server.h"
#include "wm_surface.h"
#include "wm_window.h"

#define DEFAULT_CURSOR "left_ptr"

static void handle_request_set_cursor(struct wl_listener *listener, void *data) {
  struct wm_pointer *pointer = wl_container_of(listener, pointer, request_set_cursor);
  struct wlr_seat_pointer_request_set_cursor_event *event = data;

  struct wlr_surface *focused_surface =
		event->seat_client->seat->pointer_state.focused_surface;

  if (pointer->focused_surface && pointer->focused_surface->surface != focused_surface) {
    wm_pointer_set_default_cursor(pointer);
		return;
  }

  wlr_cursor_set_surface(pointer->cursor,
    event->surface, event->hotspot_x, event->hotspot_y);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_button *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, button);

  wlr_seat_pointer_notify_button(pointer->seat->seat,
    event->time_msec, event->button, event->state);

  if (event->state == WLR_BUTTON_RELEASED) {
    wm_pointer_set_mode(pointer, WM_POINTER_MODE_FREE);
  }

  if (event->state == WLR_BUTTON_PRESSED) {
    wm_server_focus_window_under_point(pointer->server, pointer->seat,
      pointer->cursor->x, pointer->cursor->y);

    wm_pointer_motion(pointer, event->time_msec);
  }
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_motion *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, cursor_motion);
  wlr_cursor_move(pointer->cursor, event->device, event->delta_x, event->delta_y);
  wm_pointer_motion(pointer, event->time_msec);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_motion_absolute *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, cursor_motion_absolute);
  wlr_cursor_warp_absolute(pointer->cursor, event->device, event->x, event->y);
  wm_pointer_motion(pointer, event->time_msec);
}

static void handle_cursor_frame(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, cursor_frame);
  wlr_seat_pointer_notify_frame(pointer->seat->seat);
}


static void handle_axis(struct wl_listener *listener, void *data) {
  struct wm_pointer *pointer = wl_container_of(listener, pointer, axis);
	struct wlr_event_pointer_axis *event = data;

  float delta = event->delta;
  double delta_discrete = event->delta;

  bool natural_scrolling = true;

  if (natural_scrolling) {
    delta = -delta;
    delta_discrete = -delta_discrete;
  }

  wlr_seat_pointer_notify_axis(pointer->seat->seat, event->time_msec,
    event->orientation, delta, delta_discrete, event->source);
}

void wm_pointer_set_mode(struct wm_pointer* pointer, int mode) {
  pointer->mode = mode;
}

void wm_pointer_set_default_cursor(struct wm_pointer* pointer) {
  wlr_xcursor_manager_set_cursor_image(pointer->server->xcursor_manager,
		DEFAULT_CURSOR, pointer->cursor);
}

void wm_pointer_set_resize_edge(struct wm_pointer* pointer, uint32_t resize_edge) {
  pointer->resize_edge = resize_edge;
}

struct wm_window* wm_pointer_focused_window(struct wm_pointer *pointer) {
  struct wm_window* window;
  wl_list_for_each(window, &pointer->server->windows, link) {
    if (pointer->focused_surface == window->surface) {
      return window;
    }
  }
  return NULL;
}

void wm_pointer_motion(struct wm_pointer *pointer, uint32_t time) {
  wm_server_focus_window_under_point(pointer->server, pointer->seat,
      pointer->cursor->x, pointer->cursor->y);

  struct wm_window* window = wm_pointer_focused_window(pointer);

  if (!window) {
    wm_pointer_set_default_cursor(pointer);
    return;
  }

  window->update_x = false;
  window->update_y = false;

  if (pointer->mode == WM_POINTER_MODE_RESIZE) {
    wm_window_resize(window, pointer);
    return;
  }

  if (pointer->mode == WM_POINTER_MODE_MOVE) {
    int dx = pointer->cursor->x - pointer->offset_x;
    int dy = pointer->cursor->y - pointer->offset_y;

    int x = pointer->window_x + dx;
    int y = pointer->window_y + dy;

    wm_window_move(window, x, y);
  }

  double local_x = pointer->cursor->x - window->x;
  double local_y = pointer->cursor->y - window->y;

  double sx, sy;

  struct wlr_surface *surface = window->surface->wlr_surface_at(
    window->surface, local_x, local_y, &sx, &sy);

  if (surface) {
    wlr_seat_pointer_notify_enter(pointer->seat->seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(pointer->seat->seat, time, sx, sy);
  } else {
    wlr_seat_pointer_clear_focus(pointer->seat->seat);
  }
}

struct wm_pointer* wm_pointer_create(struct wm_server* server, struct wm_seat* seat) {
  struct wm_pointer* pointer = calloc(1, sizeof(struct wm_pointer));
  pointer->server = server;
  pointer->seat = seat;
  pointer->mode = WM_POINTER_MODE_FREE;
  pointer->cursor = wlr_cursor_create();

  wlr_cursor_attach_output_layout(pointer->cursor, server->layout);
  wm_pointer_set_default_cursor(pointer);

  wl_signal_add(&pointer->cursor->events.button, &pointer->button);
  pointer->button.notify = handle_cursor_button;

  wl_signal_add(&pointer->cursor->events.frame, &pointer->cursor_frame);
  pointer->cursor_frame.notify = handle_cursor_frame;

  wl_signal_add(&seat->seat->events.request_set_cursor,
    &pointer->request_set_cursor);
	pointer->request_set_cursor.notify = handle_request_set_cursor;

  wl_signal_add(&pointer->cursor->events.motion_absolute,
    &pointer->cursor_motion_absolute);

  pointer->cursor_motion_absolute.notify = handle_cursor_motion_absolute;

  wl_signal_add(&pointer->cursor->events.axis, &pointer->axis);
  pointer->axis.notify = handle_axis;

  wl_signal_add(&pointer->cursor->events.motion,
    &pointer->cursor_motion);

  pointer->cursor_motion.notify = handle_cursor_motion;

  return pointer;
}
