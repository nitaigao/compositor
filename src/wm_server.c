#include "wm_server.h"

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
#include <wlr/types/wlr_xdg_output.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_output_layout.h>

#include "wm_pointer.h"
#include "wm_seat.h"
#include "wm_window.h"
#include "wm_surface.h"
#include "wm_keyboard.h"

struct wm_seat* wm_server_find_or_create_seat(struct wm_server* server, const char* seat_name) {
  struct wm_seat *seat;
  wl_list_for_each(seat, &server->seats, link) {
    if (strcmp(seat->name, seat_name) == 0) {
      return seat;
    }
  }

  seat = calloc(1, sizeof(struct wm_seat));
  seat->seat = wlr_seat_create(server->wl_display, seat_name);
  seat->server = server;
  seat->name = strdup(seat_name);

  wl_list_insert(&server->seats, &seat->link);

  return seat;
}

void wm_server_connect_input(struct wm_server* server, struct wlr_input_device* device) {
  struct wm_seat* seat = wm_server_find_or_create_seat(server, WM_DEFAULT_SEAT);

  if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
    fprintf(stdout, "Keyboard Connected\n");

    wm_seat_attach_keyboard_device(seat, device);

    seat->seat->capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
  }

  if (device->type == WLR_INPUT_DEVICE_POINTER) {
    printf("Pointing device connected\n");

    wm_seat_attach_pointing_device(seat, device);

    seat->seat->capabilities |= WL_SEAT_CAPABILITY_POINTER;
  }

  wlr_seat_set_capabilities(seat->seat, seat->seat->capabilities);
}
