#ifndef __WM_SEAT_H
#define __WM_SEAT_H

#include <wayland-server.h>

struct wm_seat {
  const char* name;
  struct wm_server *server;
  struct wm_pointer *pointer;
  struct wlr_seat *seat;
  struct wl_list link;
};

struct wlr_input_device;

void wm_seat_attach_pointing_device(struct wm_seat* seat, struct wlr_input_device* device);

void wm_seat_attach_keyboard_device(struct wm_seat* seat, struct wlr_input_device* device);

#endif
