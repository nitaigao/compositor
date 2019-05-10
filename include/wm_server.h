#ifndef __WM_SERVER_H
#define __WM_SERVER_H

#include <wayland-server.h>

#define wl_list_first(head, pos, member) \
  wl_container_of((head)->next, pos, link)

#define wl_list_last(head, pos, member) \
  wl_container_of((head)->prev, pos, link)

struct wm_server {
  const char* socket;

  struct wl_display *wl_display;
  struct wl_event_loop *wl_event_loop;

  struct wlr_renderer *renderer;
  struct wlr_backend *backend;
  struct wlr_compositor *compositor;
  struct wlr_output_layout *layout;
  struct wlr_xwayland *xwayland;

  struct wlr_xcursor_manager *xcursor_manager;
  struct wlr_xdg_output_manager_v1* xdg_output_manager;
  struct wlr_data_device_manager *data_device_manager;
  struct wlr_server_decoration_manager *server_decoration_manager;
  struct wlr_linux_dmabuf_v1 *linux_dmabuf;

  struct wl_listener new_input;
  struct wl_listener new_output;

  struct wl_list seats;
  struct wl_list shells;
  struct wl_list outputs;
  struct wl_list windows;

  int pending_focus_index;
};

struct wlr_input_device;

struct wm_server* wm_server_create();

void wm_server_destroy(struct wm_server* server);

void wm_server_set_cursors(struct wm_server* server);

void wm_server_run(struct wm_server* server);

struct wm_seat* wm_server_find_or_create_seat(struct wm_server* server,
  const char* seat_name);

struct wm_window* wm_server_window_at_point(struct wm_server* server,
  int x, int y);

void wm_server_maximize_focused_window(struct wm_server* server);

void wm_server_switch_window(struct wm_server* server);

void wm_server_commit_window_switch(struct wm_server* server,
  struct wm_seat* seat);

void wm_server_add_window(struct wm_server* server,
  struct wm_window* window, struct wm_seat* seat);

void wm_server_remove_window(struct wm_window* window);

void wm_server_focus_window_under_point(struct wm_server* server,
  struct wm_seat* seat, double x, double y);

#endif
