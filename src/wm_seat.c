#define _POSIX_C_SOURCE 200809L

#include "wm_seat.h"

#include <stdlib.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/util/log.h>

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

void wm_seat_attach_pointing_device(struct wm_seat* seat,
  struct wlr_input_device* device) {
  if (seat->pointer == NULL) {
    seat->pointer = wm_pointer_create(seat->server, seat);
  }

  wlr_cursor_attach_input_device(seat->pointer->cursor, device);
}

void wm_seat_attach_keyboard_device(struct wm_seat* seat,
  struct wlr_input_device* device) {
  struct wm_keyboard* keyboard = wm_keyboard_create(device, seat);
  wl_list_insert(&seat->keyboards, &keyboard->link);
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
