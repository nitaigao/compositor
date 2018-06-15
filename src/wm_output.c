#include "wm_output.h"

#include <stdio.h>
#include <wlr/backend.h>
#include <wayland-server.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>

#include "wm_server.h"
#include "wm_window.h"
#include "wm_surface.h"
#include "wm_seat.h"

void wm_output_render(struct wm_output* output) {
  struct wm_server *server = output->server;
  struct wlr_output *wlr_output = output->wlr_output;
  struct wlr_renderer *renderer = wlr_backend_get_renderer(wlr_output->backend);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  wlr_output_make_current(wlr_output, NULL);
  wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

  float color[4] = {1.0, 0, 0, 1.0};
  wlr_renderer_clear(renderer, color);

  struct wm_window *window;
  wl_list_for_each_reverse(window, &server->windows, link) {
    struct wlr_surface *surface = window->surface->surface;

    if (surface == NULL) {
      continue;
    }

    if (!wlr_surface_has_buffer(surface)) {
      printf("Surface has no buffer\n");
      continue;
    }

    struct wlr_box render_box = {
      .x = window->x * output->wlr_output->scale,
      .y = window->y * output->wlr_output->scale,
      .width = surface->current->width * window->surface->scale,
      .height = surface->current->height * window->surface->scale
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
