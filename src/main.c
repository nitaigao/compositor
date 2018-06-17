#define _POSIX_C_SOURCE 199309L

#include "wm_server.h"

int main() {
  struct wm_server* server = wm_server_create();
  wm_server_run(server);
  wm_server_destroy(server);
  return 0;
}
