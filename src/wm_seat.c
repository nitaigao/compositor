#define _POSIX_C_SOURCE 200809L

#include "wm_seat.h"

#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "wm_server.h"
#include "wm_pointer.h"
#include "wm_seat.h"
#include "wm_window.h"
#include "wm_surface.h"
#include "wm_keyboard.h"
#include "wm_output.h"

void seat_destroy_notify(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_seat *seat = wl_container_of(listener, seat, destroy);
  wm_seat_destroy(seat);
}

void keyboard_modifiers_notify(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
  wlr_seat_keyboard_notify_modifiers(keyboard->seat->seat, &keyboard->device->keyboard->modifiers);
}

void keyboard_key_notify(struct wl_listener *listener, void *data) {
  struct wlr_event_keyboard_key *event = data;
  struct wm_keyboard *keyboard = wl_container_of(listener, keyboard, key);
  wm_keyboard_key_event(keyboard, event);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_button *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, button);

  if (event->state == WLR_BUTTON_RELEASED) {
    wm_pointer_set_mode(pointer, WM_POINTER_MODE_FREE);
  }

  wlr_seat_pointer_notify_button(pointer->seat->seat, event->time_msec, event->button, event->state);
}

#define wl_list_first(head, pos, member) \
  wl_container_of((head)->next, pos, link)

static void handle_motion(struct wm_pointer *pointer, uint32_t time) {
  pointer->focused_surface = NULL;

  int list_length = wl_list_length(&pointer->server->windows);

  if (list_length > 0) {
    struct wm_window *window = wl_list_first(
      &pointer->server->windows, window, link);

    window->update_x = false;
    window->update_y = false;
    pointer->focused_surface = window->surface;

    if (pointer->mode == WM_POINTER_MODE_RESIZE) {
      wm_window_resize(window, pointer);
    }

    if (pointer->mode == WM_POINTER_MODE_MOVE) {
      wm_window_move(window, pointer);
    }

    double local_x = pointer->cursor->x - window->x;
    double local_y = pointer->cursor->y - window->y;

    double sx, sy;
    struct wlr_surface *surface = wlr_surface_surface_at(
      window->surface->surface, local_x, local_y, &sx, &sy);

    if (surface) {
      wlr_seat_pointer_notify_enter(pointer->seat->seat, surface, sx, sy);
      wlr_seat_pointer_notify_motion(pointer->seat->seat, time, sx, sy);
    } else {
      wlr_seat_pointer_clear_focus(pointer->seat->seat);
    }
  }
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_motion *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, cursor_motion);
  wlr_cursor_move(pointer->cursor, event->device, event->delta_x, event->delta_y);
  handle_motion(pointer, event->time_msec);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_motion_absolute *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, cursor_motion_absolute);
  wlr_cursor_warp_absolute(pointer->cursor, event->device, event->x, event->y);
  handle_motion(pointer, event->time_msec);
}

static void handle_axis(struct wl_listener *listener, void *data) {
  struct wm_pointer *pointer = wl_container_of(listener, pointer, axis);
	struct wlr_event_pointer_axis *event = data;
  wlr_seat_pointer_notify_axis(pointer->seat->seat, event->time_msec,
    event->orientation, event->delta, event->delta_discrete, event->source);
}

static void handle_request_set_cursor(struct wl_listener *listener, void *data) {
  struct wm_pointer *pointer = wl_container_of(listener, pointer, request_set_cursor);
  struct wlr_seat_pointer_request_set_cursor_event *event = data;

  struct wlr_surface *focused_surface =
		event->seat_client->seat->pointer_state.focused_surface;

  if (pointer->focused_surface->surface != focused_surface) {
    wm_pointer_set_default_cursor(pointer);
		return;
  }

  wlr_cursor_set_surface(pointer->cursor,
    event->surface, event->hotspot_x, event->hotspot_y);
}

void wm_seat_destroy(struct wm_seat* seat) {
  struct wm_keyboard *keyboard, *tmp;
  wl_list_for_each_safe(keyboard, tmp, &seat->keyboards, link) {
    wm_keyboard_destroy(keyboard);
  }

  wlr_cursor_destroy(seat->pointer->cursor);
  free(seat->pointer);

  wl_list_remove(&seat->link);
  wl_list_remove(&seat->destroy.link);

  free(seat);
}

void wm_seat_create_pointer(struct wm_seat* seat) {
  seat->pointer = calloc(1, sizeof(struct wm_pointer));
  seat->pointer->mode = WM_POINTER_MODE_FREE;
  seat->pointer->server = seat->server;
  seat->pointer->seat = seat;
  seat->pointer->cursor = wlr_cursor_create();

  wlr_cursor_attach_output_layout(seat->pointer->cursor, seat->server->layout);

  wm_pointer_set_default_cursor(seat->pointer);

  wl_signal_add(&seat->pointer->cursor->events.motion_absolute,
    &seat->pointer->cursor_motion_absolute);

  seat->pointer->cursor_motion_absolute.notify = handle_cursor_motion_absolute;

  wl_signal_add(&seat->pointer->cursor->events.axis, &seat->pointer->axis);
  seat->pointer->axis.notify = handle_axis;

  wl_signal_add(&seat->pointer->cursor->events.motion,
    &seat->pointer->cursor_motion);

  seat->pointer->cursor_motion.notify = handle_cursor_motion;

  wl_signal_add(&seat->pointer->cursor->events.button, &seat->pointer->button);
  seat->pointer->button.notify = handle_cursor_button;

  wl_signal_add(&seat->seat->events.request_set_cursor,
    &seat->pointer->request_set_cursor);
	seat->pointer->request_set_cursor.notify = handle_request_set_cursor;
}

void wm_seat_attach_pointing_device(struct wm_seat* seat,
  struct wlr_input_device* device) {
  if (seat->pointer == NULL) {
    wm_seat_create_pointer(seat);
  }

  struct wm_output *output;
  wl_list_for_each(output, &seat->server->outputs, link) {
    wlr_cursor_map_to_output(seat->pointer->cursor, output->wlr_output);
  }

  wlr_cursor_attach_input_device(seat->pointer->cursor, device);
}

void wm_seat_attach_keyboard_device(struct wm_seat* seat,
  struct wlr_input_device* device) {
    struct wm_keyboard *keyboard = calloc(1, sizeof(struct wm_keyboard));
    keyboard->device = device;
    keyboard->seat = seat;

    wl_list_insert(&seat->keyboards, &keyboard->link);

    int repeat_rate = 25;
    int repeat_delay = 600;
    wlr_keyboard_set_repeat_info(device->keyboard, repeat_rate, repeat_delay);

    wl_signal_add(&device->keyboard->events.key, &keyboard->key);
    keyboard->key.notify = keyboard_key_notify;

    wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
    keyboard->modifiers.notify = keyboard_modifiers_notify;

    wlr_seat_set_keyboard(seat->seat, device);

    struct xkb_rule_names rules = { 0 };
    rules.rules = getenv("XKB_DEFAULT_RULES");
    rules.model = getenv("XKB_DEFAULT_MODEL");
    rules.layout = getenv("XKB_DEFAULT_LAYOUT");
    rules.variant = getenv("XKB_DEFAULT_VARIANT");
    rules.options = getenv("XKB_DEFAULT_OPTIONS");

    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    if (!context) {
      wlr_log(L_ERROR, "Failed to create XKB context");
      exit(1);
    }

    struct xkb_keymap *keymap = xkb_map_new_from_names(context,
      &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(device->keyboard, keymap);

    xkb_context_unref(context);
    xkb_keymap_unref(keymap);
}

struct wm_seat* wm_seat_find_or_create(struct wm_server* server,
  const char* seat_name) {
  struct wm_seat *seat;
  wl_list_for_each(seat, &server->seats, link) {
    if (strcmp(seat->name, seat_name) == 0) {
      return seat;
    }
  }

  seat = calloc(1, sizeof(struct wm_seat));
  seat->seat = wlr_seat_create(server->wl_display, seat_name);
  seat->server = server;
  strcpy(seat->name, seat_name);

  printf("Created seat: %s\n", seat->name);

  wl_list_init(&seat->keyboards);
  wl_list_insert(&server->seats, &seat->link);

  wl_signal_add(&seat->seat->events.destroy, &seat->destroy);
  seat->destroy.notify = seat_destroy_notify;

  return seat;
}
