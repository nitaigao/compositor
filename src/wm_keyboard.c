#include "wm_keyboard.h"

#include <stdlib.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/backend.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_seat.h>

#include "wm_server.h"
#include "wm_seat.h"

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

void wm_keyboard_key_event(struct wm_keyboard *keyboard, struct wlr_event_keyboard_key *event) {
  if (event->state == 0) {
    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->device->keyboard->xkb_state, keycode, &syms);
    for (int i = 0; i < nsyms; i++) {
      xkb_keysym_t sym = syms[i];
      if (sym == XKB_KEY_Escape) {
        printf("Quit key received\n");
        wl_display_terminate(keyboard->seat->server->wl_display);
      }
      if (sym == XKB_KEY_F1) {
        exec_command("weston-terminal");
      }
      if (sym == XKB_KEY_F2) {
        exec_command("gnome-terminal");
      }
      if (sym == XKB_KEY_F5) {
        exec_command("chrome --force-device-scale-factor=2");
      }
      if (sym == XKB_KEY_F6) {
        exec_command("weston-simple-shm");
      }
    }
  }

  wlr_seat_set_keyboard(keyboard->seat->seat, keyboard->device);

  wlr_seat_keyboard_notify_key(
    keyboard->seat->seat,
    event->time_msec,
    event->keycode,
    event->state
  );
}
