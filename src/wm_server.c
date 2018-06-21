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
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_linux_dmabuf.h>
#include <wlr/util/log.h>

#include "wm_pointer.h"
#include "wm_seat.h"
#include "wm_window.h"
#include "wm_output.h"
#include "wm_surface.h"
#include "wm_keyboard.h"

void wm_server_destroy(struct wm_server* server) {
  // wlr_xwayland_destroy(server->xwayland);
  // server->xwayland = NULL;

  wlr_data_device_manager_destroy(server->data_device_manager);
  server->data_device_manager = NULL;

  wlr_xdg_output_manager_destroy(server->xdg_output_manager);
  server->xdg_output_manager = NULL;

  wlr_primary_selection_device_manager_destroy(server->primary_selection_device_manager);
  server->primary_selection_device_manager = NULL;

  wlr_output_layout_destroy(server->layout);
  server->layout = NULL;

  wlr_compositor_destroy(server->compositor);
  server->compositor = NULL;

  wlr_xcursor_manager_destroy(server->xcursor_manager);
  server->xcursor_manager = NULL;

  wlr_backend_destroy(server->backend);
  server->backend = NULL;

  wl_display_destroy(server->wl_display);
  server->wl_display = NULL;

  free(server);
}

void wm_server_set_cursors(struct wm_server* server) {
  wlr_xcursor_manager_load(server->xcursor_manager, 1);
  // wlr_xcursor_manager_load(server->xcursor_manager, 2);

  // struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(server->xcursor_manager, "left_ptr", 1);
  // if (xcursor != NULL) {
  //   struct wlr_xcursor_image *image = xcursor->images[0];
  //   wlr_xwayland_set_cursor(
  //     server->xwayland,
  //     image->buffer,
  //     image->width * 4,
  //     image->width,
  //     image->height,
  //     image->hotspot_x,
  //     image->hotspot_y
  //   );
  // }
}

void wm_server_connect_output(struct wm_server* server, struct wlr_output* wlr_output) {
  printf("Output %s Connected\n", wlr_output->name);
  struct wm_output *output = wm_output_create(wlr_output, server->layout, server);
  wl_list_insert(&server->outputs, &output->link);
}

void wm_server_connect_input(struct wm_server* server, struct wlr_input_device* device) {
  struct wm_seat* seat = wm_seat_find_or_create(server, WM_DEFAULT_SEAT);

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

  printf("%d\n", seat->seat->capabilities);

  wlr_seat_set_capabilities(seat->seat, seat->seat->capabilities);
}


void wm_server_run(struct wm_server* server) {
  if (!wlr_backend_start(server->backend)) {
    fprintf(stderr, "Failed to start backend\n");
  }

  printf("Backend started\n");
  setenv("WAYLAND_DISPLAY", server->socket, true);
  printf("Running compositor on wayland display '%s'\n", server->socket);


  wl_display_run(server->wl_display);
}

static void new_input_notify(struct wl_listener *listener, void *data) {
  struct wlr_input_device *device = data;
  struct wm_server *server = wl_container_of(listener, server, new_input);
  wm_server_connect_input(server, device);
}

static void new_output_notify(struct wl_listener *listener, void *data) {
  struct wm_server *server = wl_container_of(listener, server, new_output);
  wm_server_connect_output(server, data);
}

void handle_xdg_shell_surface(struct wl_listener *listener, void *data) {
  struct wlr_xdg_surface *xdg_surface = data;
  struct wm_server *server = wl_container_of(listener, server, xdg_shell_surface);

  if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
    printf("Popup requested, dropping\n");
    return;
  }

  printf("New XDG Surface: Title=%s\n", xdg_surface->toplevel->title);
  wlr_xdg_surface_ping(xdg_surface);
  wm_surface_xdg_create(xdg_surface, server);
}

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data) {
  struct wlr_xdg_surface_v6 *xdg_surface = data;
  struct wm_server *server = wl_container_of(listener, server, xdg_shell_v6_surface);

  if (xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
    printf("Popup requested, dropping\n");
    return;
  }

  printf("New XDG V6 Surface: Title=%s\n", xdg_surface->toplevel->title);
  wlr_xdg_surface_v6_ping(xdg_surface);
  wm_surface_xdg_v6_create(xdg_surface, server);
}

void handle_xwayland_surface(struct wl_listener *listener, void *data) {
  struct wlr_xwayland_surface *xwayland_surface = data;
  struct wm_server *server = wl_container_of(listener, server, xwayland_surface);

  printf("New XWayland Surface\n");
  wlr_xwayland_surface_ping(xwayland_surface);
  wm_surface_xwayland_create(xwayland_surface, server);
}

struct wm_server* wm_server_create() {
  struct wm_server* server = calloc(1, sizeof(struct wm_server));

  wl_list_init(&server->windows);
  wl_list_init(&server->outputs);
  wl_list_init(&server->seats);

  server->wl_display = wl_display_create();

  if (!server->wl_display) {
    fprintf(stderr, "Failed to create display\n");
  }

  wl_display_get_event_loop(server->wl_display);

  fprintf(stdout, "Created display\n");

  server->backend = wlr_backend_autocreate(server->wl_display, NULL);

  if (!server->backend) {
    fprintf(stderr, "Failed to create backend\n");
  }

  fprintf(stdout, "Created backend\n");

  server->data_device_manager = wlr_data_device_manager_create(server->wl_display);
  server->renderer = wlr_backend_get_renderer(server->backend);
  wlr_renderer_init_wl_display(server->renderer, server->wl_display);

  server->layout = wlr_output_layout_create();
  server->xdg_output_manager = wlr_xdg_output_manager_create(server->wl_display, server->layout);
  server->compositor = wlr_compositor_create(server->wl_display, server->renderer);

  server->xdg_shell = wlr_xdg_shell_create(server->wl_display);
  wl_signal_add(&server->xdg_shell->events.new_surface, &server->xdg_shell_surface);
  server->xdg_shell_surface.notify = handle_xdg_shell_surface;

  server->xdg_shell_v6 = wlr_xdg_shell_v6_create(server->wl_display);
  wl_signal_add(&server->xdg_shell_v6->events.new_surface, &server->xdg_shell_v6_surface);
  server->xdg_shell_v6_surface.notify = handle_xdg_shell_v6_surface;

  server->xwayland = wlr_xwayland_create(server->wl_display, server->compositor, true);
  wl_signal_add(&server->xwayland->events.new_surface, &server->xwayland_surface);
  server->xwayland_surface.notify = handle_xwayland_surface;

  server->xcursor_manager = wlr_xcursor_manager_create("default", 24);

  if (!server->xcursor_manager) {
    wlr_log(L_ERROR, "Failed to load left_ptr cursor");
    return NULL;
  }

  server->server_decoration_manager = wlr_server_decoration_manager_create(server->wl_display);
  wlr_server_decoration_manager_set_default_mode(server->server_decoration_manager, WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);

  server->primary_selection_device_manager = wlr_primary_selection_device_manager_create(server->wl_display);

  server->linux_dmabuf = wlr_linux_dmabuf_create(server->wl_display, server->renderer);

  server->socket = wl_display_add_socket_auto(server->wl_display);

  if (!server->socket) {
    fprintf(stderr, "Failed to get socket\n");
  }

  setenv("_WAYLAND_DISPLAY", server->socket, true);

  wl_signal_add(&server->backend->events.new_input, &server->new_input);
  server->new_input.notify = new_input_notify;

  server->new_output.notify = new_output_notify;
  wl_signal_add(&server->backend->events.new_output, &server->new_output);

  wm_server_set_cursors(server);

  return server;
}
