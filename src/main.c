#include <GLES2/gl2.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/xwayland.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_output.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_wl_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

#include "wm_seat.h"
#include "wm_pointer.h"
#include "wm_output.h"
#include "wm_surface.h"
#include "wm_keyboard.h"
#include "wm_window.h"
#include "wm_server.h"

static void output_destroy_notify(struct wl_listener *listener, void *data) {
  printf("output_destroy_notify\n");
  struct wm_output *output = wl_container_of(listener, output, destroy);

  wl_list_remove(&output->link);
  wl_list_remove(&output->destroy.link);
  wl_list_remove(&output->frame.link);

  free(output);
}

static void output_frame_notify(struct wl_listener *listener, void *data) {
  struct wm_output *output = wl_container_of(listener, output, frame);
  wm_output_render(output);
}

static void new_output_notify(struct wl_listener *listener, void *data) {
  printf("New Output Connected\n");
  struct wlr_output *wlr_output = data;
  struct wm_server *server = wl_container_of(listener, server, new_output);

  if (!wl_list_empty(&wlr_output->modes)) {
    struct wlr_output_mode *mode = wl_container_of(wlr_output->modes.prev, mode, link);
    wlr_output_set_mode(wlr_output, mode);
  }

  printf("Output %s Connected\n", wlr_output->name);

  struct wm_output *output = calloc(1, sizeof(struct wm_output));

  clock_gettime(CLOCK_MONOTONIC, &output->last_frame);
  output->server = server;
  output->wlr_output = wlr_output;
  wl_list_insert(&server->outputs, &output->link);

  output->destroy.notify = output_destroy_notify;
  wl_signal_add(&wlr_output->events.destroy, &output->destroy);

  output->frame.notify = output_frame_notify;
  wl_signal_add(&wlr_output->events.frame, &output->frame);

  wlr_output_set_scale(wlr_output, 2.0f);
	wlr_output_set_transform(wlr_output, WL_OUTPUT_TRANSFORM_NORMAL);
	wlr_output_layout_add(server->layout, wlr_output, 0, 0);

  // wlr_output_layout_add_auto(server->layout, wlr_output);

  wlr_xcursor_manager_load(server->xcursor_manager, wlr_output->scale);

  wlr_output_create_global(wlr_output);
}

void new_input_notify(struct wl_listener *listener, void *data) {
  printf("New Input Connected\n");
  struct wlr_input_device *device = data;
  struct wm_server *server = wl_container_of(listener, server, new_input);
  wm_server_connect_input(server, device);
}

void handle_map(struct wl_listener *listener, void *data) {
  printf("handle_map\n");
  struct wm_surface *surface = wl_container_of(listener, surface, map);

  printf("map\n");

  struct wm_window *window = calloc(1, sizeof(struct wm_window));
  window->x = 0;
  window->y = 0;
  window->surface = surface;

  surface->window = window;

  wl_list_insert(&surface->server->windows, &window->link);

  struct wm_seat *seat = wm_server_find_or_create_seat(window->surface->server, WM_DEFAULT_SEAT);

  wlr_seat_keyboard_notify_enter(
    seat->seat,
    surface->surface,
    NULL, 0, NULL
  );
}

void handle_unmap(struct wl_listener *listener, void *data) {
  printf("handle_unmap\n");
  struct wm_surface *wm_surface = wl_container_of(listener, wm_surface, unmap);

  wl_list_remove(&wm_surface->window->link);

  int list_length = wl_list_length(&wm_surface->server->windows);

  if (list_length > 0) {
    struct wm_window *window;
    wl_list_for_each(window, &wm_surface->server->windows, link) {
      break;
    }

    struct wm_seat *seat = wm_server_find_or_create_seat(window->surface->server, WM_DEFAULT_SEAT);

    wlr_seat_keyboard_notify_enter(
      seat->seat,
      window->surface->surface,
      NULL, 0, NULL
    );

    wlr_xdg_toplevel_v6_set_activated(window->surface->xdg_surface_v6, true);
  }
}

void handle_move(struct wl_listener *listener, void *data) {
  printf("handle move\n");
  struct wm_surface *surface = wl_container_of(listener, surface, move);
  struct wlr_wl_shell_surface_move_event *event = data;
  struct wm_seat *seat = wm_server_find_or_create_seat(surface->server, event->seat->seat->name);
  seat->pointer->mode = WM_POINTER_MODE_MOVE;
}

void handle_xwayland_surface(struct wl_listener *listener, void *data) {
  struct wlr_xwayland_surface *xwayland_surface = data;
  struct wm_server *server = wl_container_of(listener, server, xwayland_surface);

  printf("New XWayland Surface\n");

  wlr_xwayland_surface_ping(xwayland_surface);

  struct wm_surface *wm_surface = calloc(1, sizeof(struct wm_surface));
  wm_surface->server = server;
  wm_surface->xwayland_surface = xwayland_surface;
  wm_surface->surface = xwayland_surface->surface;

  wm_surface->map.notify = handle_map;
  wl_signal_add(&xwayland_surface->events.map, &wm_surface->map);

  // wlr_xwayland_surface_activate(xwayland_surface, true);
 }

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data) {
  struct wlr_xdg_surface_v6 *xdg_surface = data;
  struct wm_server *server = wl_container_of(listener, server, xdg_shell_v6_surface);

  if (xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
    printf("Popup requested, dropping\n");
    return;
  }

  printf("New XDG Top Level Surface: Title=%s\n", xdg_surface->toplevel->title);
  wlr_xdg_surface_v6_ping(xdg_surface);

  struct wm_surface *wm_surface = calloc(1, sizeof(struct wm_surface));
  wm_surface->server = server;
  wm_surface->xdg_surface_v6 = xdg_surface;
  wm_surface->surface = xdg_surface->surface;

  if (!wm_surface) {
    fprintf(stderr, "Failed to created surface\n");
  }

  wlr_xdg_toplevel_v6_set_activated(wm_surface->xdg_surface_v6, true);

  wm_surface->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &wm_surface->map);

  wm_surface->unmap.notify = handle_unmap;
  wl_signal_add(&xdg_surface->events.unmap, &wm_surface->unmap);

  wm_surface->move.notify = handle_move;
  wl_signal_add(&xdg_surface->toplevel->events.request_move, &wm_surface->move);
}

int main() {
  struct wm_server server;
  wl_list_init(&server.windows);
  wl_list_init(&server.seats);

  server.wl_display = wl_display_create();

  if (!server.wl_display) {
    fprintf(stderr, "Failed to create display\n");
  }

  fprintf(stdout, "Created display\n");

  server.backend = wlr_backend_autocreate(server.wl_display, NULL);

  if (!server.backend) {
    fprintf(stderr, "Failed to create backend\n");
  }

  fprintf(stdout, "Created backend\n");

  server.xcursor_manager = wlr_xcursor_manager_create("default", 24);

  if (!server.xcursor_manager) {
    wlr_log(L_ERROR, "Failed to load left_ptr cursor");
    return 1;
  }

  wl_signal_add(&server.backend->events.new_input, &server.new_input);
  server.new_input.notify = new_input_notify;

  wl_list_init(&server.outputs);
  server.new_output.notify = new_output_notify;
  wl_signal_add(&server.backend->events.new_output, &server.new_output);

  const char *socket = wl_display_add_socket_auto(server.wl_display);

  if (!socket) {
    fprintf(stderr, "Failed to get socket\n");
  }

  fprintf(stdout, "Got socket\n");

  server.renderer = wlr_backend_get_renderer(server.backend);
  server.compositor = wlr_compositor_create(server.wl_display, server.renderer);

  wlr_renderer_init_wl_display(server.renderer, server.wl_display);
  wlr_primary_selection_device_manager_create(server.wl_display);

  server.layout = wlr_output_layout_create();
  wlr_xdg_output_manager_create(server.wl_display, server.layout);

  server.xdg_shell_v6 = wlr_xdg_shell_v6_create(server.wl_display);
  wl_signal_add(&server.xdg_shell_v6->events.new_surface, &server.xdg_shell_v6_surface);
  server.xdg_shell_v6_surface.notify = handle_xdg_shell_v6_surface;

  server.data_device_manager = wlr_data_device_manager_create(server.wl_display);

  server.xwayland = wlr_xwayland_create(server.wl_display, server.compositor, false);
  wl_signal_add(&server.xwayland->events.new_surface, &server.xwayland_surface);
  server.xwayland_surface.notify = handle_xwayland_surface;

  if (!wlr_backend_start(server.backend)) {
    fprintf(stderr, "Failed to start backend\n");
    wl_display_destroy(server.wl_display);
    return 1;
  }

  fprintf(stdout, "Backend started\n");

  printf("Running compositor on wayland display '%s'\n", socket);
  setenv("WAYLAND DISPLAY", socket, true);

  wl_display_run(server.wl_display);

  wl_display_destroy(server.wl_display);

  // wlr_xcursor_manager_destroy(input.xcursor_manager);
  // wlr_cursor_destroy(input.cursor);
  // wlr_output_layout_destroy(server.layout);

  return 0;
}
