#ifndef __WM_OUTPUT_H
#define __WM_OUTPUT_H

#include <time.h>
#include <wayland-server.h>

struct wm_output {
  struct wm_server *server;

  struct wlr_output *wlr_output;

  struct wl_listener destroy;
  struct wl_listener frame;
  struct wl_list link;

  struct timespec last_frame;
};

void wm_output_render(struct wm_output* output);

#endif
