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
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

enum roots_view_type {
	WM_XDG_SHELL_V6_VIEW,
	WM_XWAYLAND_VIEW,
};
struct wm_server {
  struct wl_display *wl_display;
  struct wlr_renderer *renderer;
  struct wl_event_loop *wl_event_loop;
  struct wlr_backend *backend;
  struct wlr_compositor *compositor;
  struct wl_listener new_input;
  struct wl_listener new_output;
  struct wl_list outputs;
  struct wlr_output_layout *layout;
  struct wlr_xwayland *xwayland;
  struct wl_listener xwayland_surface;
  struct wlr_xdg_shell_v6 *xdg_shell_v6;
  struct wl_listener xdg_shell_v6_surface;
  struct wlr_xcursor_manager *xcursor_manager;
  struct wlr_seat *seat;
  struct wl_list windows;
};

struct wm_output {
  struct wlr_output *wlr_output;
  struct wm_server *server;
  struct timespec last_frame;
  struct wl_listener destroy;
  struct wl_listener frame;
  struct wl_list link;
};

struct wm_surface {
  struct wl_listener map;
  struct wl_listener unmap;
  struct wm_server *server;
  struct wlr_xdg_surface_v6 *xdg_surface_v6;
  struct wlr_xwayland_surface *xwayland_surface;
  struct wlr_surface *surface;
  struct wl_listener commit;
  struct wm_window *window;
};

struct wm_keyboard {
  struct wm_server *server;
  struct wlr_input_device *device;
  struct wl_listener key;
  struct wl_listener modifiers;
  struct wl_listener destroy;
};

int WM_POINTER_MODE_FREE = 0;
int WM_POINTER_MODE_MOVE = 1;

struct wm_pointer {
  struct wlr_cursor *cursor;
  struct wm_server *server;
  struct wl_listener cursor_motion_absolute;
  struct wl_listener cursor_motion;
  struct wl_listener button;
  int mode;
  double delta_x;
  double delta_y;
  double last_x;
  double last_y;
};

struct wm_window {
  int x;
  int y;
  struct wm_surface *surface;
  struct wl_list link;
};

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
  struct wm_server *server = output->server;
  struct wlr_output *wlr_output = output->wlr_output;
  struct wlr_renderer *renderer = wlr_backend_get_renderer(wlr_output->backend);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  wlr_output_make_current(wlr_output, NULL);
  wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

  float color[4] = {1.0, 0, 0, 1.0};
  wlr_renderer_clear(renderer, color);

  int scale = 2;

  struct wm_window *window;
  wl_list_for_each_reverse(window, &server->windows, link) {
    struct wlr_surface *surface = window->surface->surface;

    if (!wlr_surface_has_buffer(surface)) {
      printf("Surface has no buffer\n");
      continue;
    }

    struct wlr_box render_box = {
      .x = window->x,
      .y = window->y,
      .width = surface->current->width * scale,
      .height = surface->current->height * scale
    };

    float matrix[16];

    wlr_matrix_project_box(
      matrix,
      &render_box,
      surface->current->transform,
      0,
      wlr_output->transform_matrix
    );

    wlr_render_texture_with_matrix(renderer, surface->texture, matrix, 1.0f);
    wlr_surface_send_frame_done(surface, &now);
  }

  wlr_output_swap_buffers(wlr_output, NULL, NULL);
  wlr_renderer_end(renderer);
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

  wlr_output_layout_add_auto(server->layout, wlr_output);

  wlr_xcursor_manager_load(server->xcursor_manager, wlr_output->scale);

  wlr_output_create_global(wlr_output);
}

void keyboard_destroy_notify(struct wl_listener *listener, void *data) {
  printf("Destroy Keyboard\n");
  struct wm_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
  wl_list_remove(&keyboard->destroy.link);
  wl_list_remove(&keyboard->key.link);
  free(keyboard);
}

void exec_command(const char* shell_cmd) {
  printf("Execute %s\n", shell_cmd);
  pid_t pid = fork();
  if (pid < 0) {
    wlr_log(L_ERROR, "cannot execute binding command: fork() failed");
    return;
  } else if (pid == 0) {
    execl("/bin/sh", "/bin/sh", "-c", shell_cmd, (void *)NULL);
  }
}

void keyboard_modifiers_notify(struct wl_listener *listener, void *data) {
  struct wlr_event_keyboard_key *event = data;
  struct wm_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);

  wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &keyboard->device->keyboard->modifiers);
}

void keyboard_key_notify(struct wl_listener *listener, void *data) {
  struct wlr_event_keyboard_key *event = data;
  struct wm_keyboard *keyboard = wl_container_of(listener, keyboard, key);

  if (event->state == 0) {
    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->device->keyboard->xkb_state, keycode, &syms);
    for (int i = 0; i < nsyms; i++) {
      xkb_keysym_t sym = syms[i];
      if (sym == XKB_KEY_Escape) {
        printf("Quit key received\n");
        wl_display_terminate(keyboard->server->wl_display);
      }
      if (sym == XKB_KEY_F1) {
        exec_command("weston-terminal");
      }
      if (sym == XKB_KEY_F2) {
        exec_command("weston-editor");
      }
      if (sym == XKB_KEY_F5) {
        exec_command("chrome");
      }
      if (sym == XKB_KEY_F6) {
        exec_command("weston-simple-shm");
      }
    }
  }

  wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);

  wlr_seat_keyboard_notify_key(
    keyboard->server->seat,
    event->time_msec,
    event->keycode,
    event->state
  );
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
  printf("Cursor button\n");
  struct wlr_event_pointer_button *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, button);
  if (event->state == WLR_BUTTON_PRESSED) {
    pointer->mode = WM_POINTER_MODE_MOVE;
  }
  if (event->state == WLR_BUTTON_RELEASED) {
    pointer->mode = WM_POINTER_MODE_FREE;
  }
}

static void handle_motion(struct wm_pointer *pointer) {
  pointer->delta_x = pointer->cursor->x - pointer->last_x;
  pointer->delta_y = pointer->cursor->y - pointer->last_y;
  pointer->last_x = pointer->cursor->x;
  pointer->last_y = pointer->cursor->y;

  if (pointer->mode == WM_POINTER_MODE_MOVE) {
    int list_length = wl_list_length(&pointer->server->windows);

    if (list_length > 0) {
      struct wm_window *window;
      wl_list_for_each(window, &pointer->server->windows, link) {
        break;
      }
      window->x += pointer->delta_x;
      window->y += pointer->delta_y;
    }
  }
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_motion *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, cursor_motion);
  wlr_cursor_move(pointer->cursor, event->device, event->delta_x, event->delta_y);
  handle_motion(pointer);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_motion_absolute *event = data;
  struct wm_pointer *pointer = wl_container_of(listener, pointer, cursor_motion_absolute);
  wlr_cursor_warp_absolute(pointer->cursor, event->device, event->x, event->y);
  handle_motion(pointer);
}

void new_input_notify(struct wl_listener *listener, void *data) {
  printf("New Input Connected\n");
  struct wlr_input_device *device = data;
  struct wm_server *server = wl_container_of(listener, server, new_input);

  if (!server->seat) {
    printf("Created seat\n");
    server->seat = wlr_seat_create(server->wl_display, "seat0");
  }

  if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
    struct wm_keyboard *keyboard = calloc(1, sizeof(struct wm_keyboard));
    keyboard->device = device;
    keyboard->server = server;

    int repeat_rate = 25;
    int repeat_delay = 600;
    wlr_keyboard_set_repeat_info(device->keyboard, repeat_rate, repeat_delay);

    wl_signal_add(&device->keyboard->events.key, &keyboard->key);
    keyboard->key.notify = keyboard_key_notify;

    wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
    keyboard->modifiers.notify = keyboard_modifiers_notify;

    wlr_seat_set_keyboard(keyboard->server->seat, device);

    fprintf(stdout, "Keyboard Connected\n");

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

    struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(device->keyboard, keymap);
    xkb_context_unref(context);

    server->seat->capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
  }

  if (device->type == WLR_INPUT_DEVICE_POINTER) {
    printf("Pointer Connected\n");

    struct wm_pointer *pointer = calloc(1, sizeof(struct wm_pointer));
    pointer->mode = WM_POINTER_MODE_FREE;
    pointer->server = server;
    pointer->cursor = wlr_cursor_create();
    pointer->last_x = 0;
    pointer->last_y = 0;
    pointer->delta_x = 0;
    pointer->delta_y = 0;

    wlr_cursor_attach_output_layout(pointer->cursor, server->layout);
    wlr_cursor_attach_input_device(pointer->cursor, device);
    wlr_xcursor_manager_set_cursor_image(server->xcursor_manager, "left_ptr", pointer->cursor);

    wl_signal_add(&pointer->cursor->events.motion_absolute, &pointer->cursor_motion_absolute);
    pointer->cursor_motion_absolute.notify = handle_cursor_motion_absolute;

    wl_signal_add(&pointer->cursor->events.motion, &pointer->cursor_motion);
    pointer->cursor_motion.notify = handle_cursor_motion;

    wl_signal_add(&pointer->cursor->events.button, &pointer->button);
	  pointer->button.notify = handle_cursor_button;

    server->seat->capabilities |= WL_SEAT_CAPABILITY_POINTER;
  }

  wlr_seat_set_capabilities(server->seat, server->seat->capabilities);
}

void handle_map(struct wl_listener *listener, void *data) {
  printf("handle_map\n");
  struct wm_surface *wm_surface = wl_container_of(listener, wm_surface, map);

  wlr_seat_keyboard_notify_enter(
    wm_surface->server->seat,
    wm_surface->surface,
    NULL, 0, NULL
  );

  struct wm_window *window = calloc(1, sizeof(struct wm_window));
  window->x = 0;
  window->y = 0;
  window->surface = wm_surface;

  wm_surface->window = window;

  wl_list_insert(&wm_surface->server->windows, &window->link);
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

    wlr_seat_keyboard_notify_enter(
      window->surface->server->seat,
      window->surface->surface,
      NULL, 0, NULL
    );

    // wlr_xdg_toplevel_v6_set_activated(window->surface->xdg_surface_v6, true);
  }
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
}

int main() {
  struct wm_server server;
  wl_list_init(&server.windows);
  server.seat = 0;

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
  server.compositor = wlr_compositor_create(server.wl_display,  server.renderer);

  wlr_renderer_init_wl_display(server.renderer, server.wl_display);
  wlr_primary_selection_device_manager_create(server.wl_display);

  server.layout = wlr_output_layout_create();
  wlr_xdg_output_manager_create(server.wl_display, server.layout);

  server.xdg_shell_v6 = wlr_xdg_shell_v6_create(server.wl_display);
  wl_signal_add(&server.xdg_shell_v6->events.new_surface, &server.xdg_shell_v6_surface);
  server.xdg_shell_v6_surface.notify = handle_xdg_shell_v6_surface;

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
