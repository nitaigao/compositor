#ifndef __WM_KEYBOARD_H
#define __WM_KEYBOARD_H

#include <wayland-server.h>

struct wm_keyboard {
  struct wm_seat *seat;
  struct wlr_input_device *device;
  struct wl_listener key;
  struct wl_listener modifiers;
  struct wl_listener destroy;
  struct wl_list link;
};

struct wlr_event_keyboard_key;

void wm_keyboard_destroy(struct wm_keyboard* keyboard);

void wm_keyboard_key_event(struct wm_keyboard *keyboard,
  struct wlr_event_keyboard_key *event);

#endif
