#include <GLES2/gl2.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

struct mcw_server {
  struct wl_display *wl_display;
  struct wl_event_loop *wl_event_loop;
  struct wlr_backend *backend;
  struct wl_listener new_output;
  struct wl_list outputs;
};

struct mcw_output {
  struct wlr_output *wlr_output;
  struct mcw_server *server;
  struct timespec last_frame;
  struct wl_listener destroy;
  struct wl_listener frame;
  struct wl_list link;
};

static void output_destroy_notify(struct wl_listener *listener, void *data) {
  struct mcw_output *output = wl_container_of(listener, output, destroy);

  wl_list_remove(&output->link);
  wl_list_remove(&output->destroy.link);
  wl_list_remove(&output->frame.link);

  free(output);
}

static void output_frame_notify(struct wl_listener *listener, void *data) {
  struct mcw_output *output = wl_container_of(listener, output, frame);
  struct wlr_output *wlr_output = data;
  struct wlr_renderer *renderer = wlr_backend_get_renderer(wlr_output->backend);

  wlr_output_make_current(wlr_output, NULL);
  wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

  float color[4] = {1.0, 0, 0, 1.0};
  wlr_renderer_clear(renderer, color);

  wlr_output_swap_buffers(wlr_output, NULL, NULL);
  wlr_renderer_end(renderer);
}

static void new_output_notify(struct wl_listener *listener, void *data) {
  struct mcw_server *server = wl_container_of(listener, server, new_output);
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

  wl_list_init(&server.outputs);
  server.new_output.notify = new_output_notify;
  wl_signal_add(&server.backend->events.new_output, &server.new_output);

  if (!wlr_backend_start(server.backend)) {
    fprintf(stderr, "Failed to start backend\n");
    wl_display_destroy(server.wl_display);
    return 1;
  }

  fprintf(stdout, "Backend started\n");

  wl_display_run(server.wl_display);

  wl_display_destroy(server.wl_display);

  return 0;
}
