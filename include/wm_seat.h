#ifndef __WM_SEAT_H
#define __WM_SEAT_H

#include <wayland-server.h>

#define WM_DEFAULT_SEAT "seat0"
#define WM_SEAT_NAME_SIZE 256

struct wm_seat {
  char name[WM_SEAT_NAME_SIZE];
  struct wm_server *server;
  struct wm_pointer *pointer;
  struct wlr_seat *seat;
  struct wl_list keyboards;
  struct wl_list link;
  struct wl_listener destroy;
};

struct wlr_input_device;

void wm_seat_destroy(struct wm_seat* seat);
void wm_seat_destroy(struct wm_seat* seat);

struct wm_seat* wm_seat_find_or_create(struct wm_server* server,
  const char* name);

void wm_seat_attach_pointing_device(struct wm_seat* seat,
  struct wlr_input_device* device);

void wm_seat_attach_keyboard_device(struct wm_seat* seat,
  struct wlr_input_device* device);

#endif
