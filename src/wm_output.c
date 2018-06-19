#include "wm_output.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <wlr/backend.h>
#include <wayland-server.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "wm_server.h"
#include "wm_window.h"
#include "wm_surface.h"
#include "wm_seat.h"

void wm_output_destroy(struct wm_output* output) {
  free(output);
}

static void output_frame_notify(struct wl_listener *listener, void *data) {
  struct wm_output *output = wl_container_of(listener, output, frame);
  wm_output_render(output);
}

static void output_destroy_notify(struct wl_listener *listener, void *data) {
  printf("output_destroy_notify\n");
  struct wm_output *output = wl_container_of(listener, output, destroy);

  wl_list_remove(&output->link);
  wl_list_remove(&output->destroy.link);
  wl_list_remove(&output->frame.link);
  wm_output_destroy(output);
}

struct wm_output* wm_output_create(struct wlr_output* wlr_output, struct wlr_output_layout *layout, struct wm_server *server) {
  if (!wl_list_empty(&wlr_output->modes)) {
    struct wlr_output_mode *mode = wl_container_of(wlr_output->modes.prev, mode, link);
    wlr_output_set_mode(wlr_output, mode);
  }

  struct wm_output *output = calloc(1, sizeof(struct wm_output));

  clock_gettime(CLOCK_MONOTONIC, &output->last_frame);
  output->server = server;
  output->wlr_output = wlr_output;

  output->destroy.notify = output_destroy_notify;
  wl_signal_add(&wlr_output->events.destroy, &output->destroy);

  output->frame.notify = output_frame_notify;
  wl_signal_add(&wlr_output->events.frame, &output->frame);

  wlr_output_set_scale(wlr_output, 2);
  wlr_output_layout_add_auto(layout, wlr_output);
  wlr_output_create_global(wlr_output);

  return output;
}

struct render_data {
  struct wm_output *output;
  struct wm_window* window;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
  if (!wlr_surface_has_buffer(surface)) {
		return;
	}

  struct render_data *render_data = data;
  struct wm_window* window = render_data->window;
  struct wm_output* output = render_data->output;

  struct wlr_box box = {
    .x = (window->x + sx) * output->wlr_output->scale,
    .y = (window->y + sy) * output->wlr_output->scale,
    .width = surface->current->width * window->surface->scale,
    .height = surface->current->height * window->surface->scale
  };

  float matrix[16];

  enum wl_output_transform transform = wlr_output_transform_invert(surface->current->transform);
	wlr_matrix_project_box(matrix, &box, transform, 0, output->wlr_output->transform_matrix);

  struct wlr_renderer *renderer = wlr_backend_get_renderer(output->wlr_output->backend);
  wlr_render_texture_with_matrix(renderer, surface->texture, matrix, 1.0f);
}

void wm_output_render(struct wm_output* output) {
  struct wm_server *server = output->server;
  struct wlr_output *wlr_output = output->wlr_output;

  struct wlr_renderer *renderer = wlr_backend_get_renderer(wlr_output->backend);

  wlr_output_make_current(wlr_output, NULL);
  wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

  float color[4] = { 1.0, 0, 0, 1.0 };
  wlr_renderer_clear(renderer, color);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  struct wm_window *window;
  wl_list_for_each_reverse(window, &server->windows, link) {
      struct render_data render_data = {
        .output = output,
        .window = window
      };

    if (window->surface->type == WM_SURFACE_TYPE_XDG) {
      wlr_xdg_surface_for_each_surface(window->surface->xdg_surface, render_surface, &render_data);
    }

    if (window->surface->type == WM_SURFACE_TYPE_XDG_V6) {
      wlr_xdg_surface_v6_for_each_surface(window->surface->xdg_surface_v6, render_surface, &render_data);
    }
  }

  wl_list_for_each_reverse(window, &server->windows, link) {
    wlr_surface_send_frame_done(window->surface->surface, &now);
  }

  wlr_output_swap_buffers(wlr_output, NULL, NULL);
  wlr_renderer_end(renderer);
}
