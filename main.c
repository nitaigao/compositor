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
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_screenshooter.h>
#include <wlr/types/wlr_gamma_control.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

struct mcw_server {
  struct wl_display *wl_display;
  struct wl_event_loop *wl_event_loop;
  struct wlr_backend *backend;
  struct wlr_compositor *compositor;
  struct wl_listener new_output;
  struct wl_list outputs;
  struct wl_listener new_input;
  struct wlr_cursor *cursor;
  struct wlr_xwayland *xwayland;
  struct wl_listener xwayland_surface;
  struct wlr_xcursor_manager *xcursor_manager;
  struct wlr_output_layout *layout;
  struct wl_listener cursor_motion_absolute;
  struct wl_listener cursor_motion;
};

struct mcw_output {
  struct wlr_output *wlr_output;
  struct mcw_server *server;
  struct timespec last_frame;
  struct wl_listener destroy;
  struct wl_listener frame;
  struct wl_list link;
};

struct sample_keyboard {
  struct mcw_server *server;
	struct wlr_input_device *device;
	struct wl_listener key;
	struct wl_listener destroy;
};

static void output_destroy_notify(struct wl_listener *listener, void *data) {
  printf("output_destroy_notify\n");
  struct mcw_output *output = wl_container_of(listener, output, destroy);

  wl_list_remove(&output->link);
  wl_list_remove(&output->destroy.link);
  wl_list_remove(&output->frame.link);

  free(output);
}

static void output_frame_notify(struct wl_listener *listener, void *data) {
  struct mcw_output *output;
  output = wl_container_of(listener, output, frame);
  struct mcw_server *server = output->server;
  struct wlr_output *wlr_output = output->wlr_output;
  struct wlr_renderer *renderer = wlr_backend_get_renderer(wlr_output->backend);

  struct timespec now;
 	clock_gettime(CLOCK_MONOTONIC, &now);

  wlr_output_make_current(wlr_output, NULL);
  wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

  float color[4] = {1.0, 0, 0, 1.0};
  wlr_renderer_clear(renderer, color);

  struct wl_resource *_surface;
  wl_resource_for_each(_surface, &server->compositor->surface_resources) {
    struct wlr_surface *surface = wlr_surface_from_resource(_surface);

    if (!wlr_surface_has_buffer(surface)) {
      continue;
    }

    struct wlr_box render_box = {
      .x = 20,
      .y = 20,
      .width = surface->current->width,
      .height = surface->current->height
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
  struct mcw_server *server;
  server = wl_container_of(listener, server, new_output);
  struct wlr_output *wlr_output = data;

  if (!wl_list_empty(&wlr_output->modes)) {
    struct wlr_output_mode *mode = wl_container_of(wlr_output->modes.prev, mode, link);
    wlr_output_set_mode(wlr_output, mode);
  }

  struct mcw_output *output = calloc(1, sizeof(struct mcw_output));
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
	wlr_xcursor_manager_set_cursor_image(server->xcursor_manager, "left_ptr", server->cursor);

  wlr_output_create_global(wlr_output);
}

void keyboard_destroy_notify(struct wl_listener *listener, void *data) {
  printf("Destroy Keyboard\n");
	struct sample_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->key.link);
	free(keyboard);
}

void exec_command(const char* shell_cmd) {
  pid_t pid = fork();
  if (pid < 0) {
    wlr_log(L_ERROR, "cannot execute binding command: fork() failed");
    return;
  } else if (pid == 0) {
    execl("/bin/sh", "/bin/sh", "-c", shell_cmd, (void *)NULL);
  }
}

void keyboard_key_notify(struct wl_listener *listener, void *data) {
  struct sample_keyboard *keyboard = wl_container_of(listener, keyboard, key);

	struct wlr_event_keyboard_key *event = data;
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
      exec_command("gnome-calendar");
    }
    if (sym == XKB_KEY_F2) {
      exec_command("gnome-terminal");
    }
    if (sym == XKB_KEY_F3) {
      exec_command("konsole");
    }
    if (sym == XKB_KEY_F4) {
      exec_command("weston-terminal");
    }
    if (sym == XKB_KEY_F5) {
      exec_command("chrome");
    }
    if (sym == XKB_KEY_F5) {
      exec_command("weston-simple-shm");
    }
	}
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct mcw_server *server;
  server = wl_container_of(listener, server, cursor_motion);
	struct wlr_event_pointer_motion *event = data;
	wlr_cursor_move(server->cursor, event->device, event->delta_x, event->delta_y);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
  struct wlr_event_pointer_motion_absolute *event = data;
  struct mcw_server *server;
  server = wl_container_of(listener, server, cursor_motion_absolute);
  wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);
}

void handle_xwayland_surface(struct wl_listener *listener, void *data) {
  printf("handle_xwayland_surface\n");
}

void new_input_notify(struct wl_listener *listener, void *data) {
	struct wlr_input_device *device = data;

  struct mcw_server *server;
  server = wl_container_of(listener, server, new_input);

  if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
    struct sample_keyboard *keyboard = calloc(1, sizeof(struct sample_keyboard));
    keyboard->device = device;
    keyboard->server = server;

    wl_signal_add(&device->keyboard->events.key, &keyboard->key);
    keyboard->key.notify = keyboard_key_notify;

    keyboard->destroy.notify = keyboard_destroy_notify;
		wl_signal_add(&device->keyboard->events.key, &keyboard->key);

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

		wlr_keyboard_set_keymap(device->keyboard, xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS));
		xkb_context_unref(context);
  }

  if (device->type == WLR_INPUT_DEVICE_POINTER) {
    printf("Pointer Connected\n");
    printf("%p\n", server);
    wlr_cursor_attach_input_device(server->cursor, device);
  }
}

int main() {
  struct mcw_server server;

  server.wl_display = wl_display_create();

  if (!server.wl_display) {
    fprintf(stderr, "Failed to create display\n");
  }

  fprintf(stdout, "Created display\n");

  server.wl_event_loop = wl_display_get_event_loop(server.wl_display);

  if (!server.wl_event_loop) {
    fprintf(stderr, "Failed to get event loop\n");
  }

  fprintf(stdout, "Got event loop\n");

  server.backend = wlr_backend_autocreate(server.wl_display, NULL);

  if (!server.backend) {
    fprintf(stderr, "Failed to create backend\n");
  }

  fprintf(stdout, "Created backend\n");

  server.cursor = wlr_cursor_create();
  printf("%p\n", server.cursor->state);
  server.layout = wlr_output_layout_create();

  printf("Cursor created\n");

  wlr_cursor_attach_output_layout(server.cursor, server.layout);

  server.xcursor_manager = wlr_xcursor_manager_create("default", 24);

  if (!server.xcursor_manager) {
		wlr_log(L_ERROR, "Failed to load left_ptr cursor");
		return 1;
	}

	wlr_xcursor_manager_set_cursor_image(server.xcursor_manager, "left_ptr", server.cursor);

	wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
	server.cursor_motion_absolute.notify = handle_cursor_motion_absolute;

  wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion.notify = handle_cursor_motion;

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

  if (!wlr_backend_start(server.backend)) {
    fprintf(stderr, "Failed to start backend\n");
    wl_display_destroy(server.wl_display);
    return 1;
  }

  fprintf(stdout, "Backend started\n");

  printf("Running compositor on wayland display '%s'\n", socket);
  setenv("WAYLAND DISPLAY", socket, true);

  server.compositor = wlr_compositor_create(server.wl_display, wlr_backend_get_renderer(server.backend));

  wlr_seat_create(server.wl_display, "seat0");
  wl_display_init_shm(server.wl_display);
  wlr_gamma_control_manager_create(server.wl_display);
  wlr_screenshooter_create(server.wl_display);
  wlr_primary_selection_device_manager_create(server.wl_display);
  wlr_idle_create(server.wl_display);
  wlr_data_device_manager_create(server.wl_display);
  wlr_xdg_shell_v6_create(server.wl_display);

  server.xwayland = wlr_xwayland_create(server.wl_display, server.compositor, false);
  wl_signal_add(&server.xwayland->events.new_surface, &server.xwayland_surface);
	server.xwayland_surface.notify = handle_xwayland_surface;

  wl_display_run(server.wl_display);

  wl_display_destroy(server.wl_display);

  wlr_xcursor_manager_destroy(server.xcursor_manager);
	wlr_cursor_destroy(server.cursor);
	wlr_output_layout_destroy(server.layout);

  return 0;
}
