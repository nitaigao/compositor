#ifndef __WM_SERVER_H
#define __WM_SERVER_H

#include <wayland-server.h>

#define WM_POINTER_MODE_FREE 0
#define WM_POINTER_MODE_MOVE 1

#define WM_DEFAULT_SEAT "seat0"

struct wm_server {
  struct wl_display *wl_display;
  struct wl_event_loop *wl_event_loop;

  struct wlr_renderer *renderer;
  struct wlr_backend *backend;
  struct wlr_compositor *compositor;
  struct wlr_output_layout *layout;
  struct wlr_xwayland *xwayland;
  struct wlr_xdg_shell_v6 *xdg_shell_v6;
  struct wlr_xdg_shell *xdg_shell;
  struct wlr_xcursor_manager *xcursor_manager;
  struct wlr_xdg_output_manager* xdg_output_manager;
  struct wlr_data_device_manager *data_device_manager;
  struct wlr_primary_selection_device_manager *primary_selection_device_manager;

  struct wl_listener xwayland_surface;
  struct wl_listener xdg_shell_v6_surface;
  struct wl_listener xdg_shell_surface;
  struct wl_listener new_input;
  struct wl_listener new_output;
  struct wl_list windows;
  struct wl_list seats;
  struct wl_list outputs;
  const char* socket;
};

struct wlr_input_device;

struct wm_server* wm_server_create();

void wm_server_destroy(struct wm_server* server);

void wm_server_set_cursors(struct wm_server* server);

void wm_server_run(struct wm_server* server);

struct wm_seat* wm_server_find_or_create_seat(struct wm_server* server, const char* seat_name);

#endif
