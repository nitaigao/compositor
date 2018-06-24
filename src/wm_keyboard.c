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

void wm_keyboard_destroy(struct wm_keyboard* keyboard) {
  wl_list_remove(&keyboard->link);
  free(keyboard);
}

void exec_command(const char* shell_cmd) {
  printf("Executing: %s\n", shell_cmd);
  pid_t pid = fork();
  if (pid < 0) {
    wlr_log(L_ERROR, "cannot execute binding command: fork() failed");
    return;
  } else if (pid == 0) {
    execl("/bin/sh", "/bin/sh", "-c", shell_cmd, (void *)NULL);
  }
}

void wm_keyboard_key_event(struct wm_keyboard *keyboard,
  struct wlr_event_keyboard_key *event) {

  struct xkb_state* state = keyboard->device->keyboard->xkb_state;
  struct xkb_keymap* keymap = keyboard->device->keyboard->keymap;

  xkb_mod_index_t super_index = xkb_keymap_mod_get_index(
    keymap, XKB_MOD_NAME_LOGO);

  int super = xkb_state_mod_index_is_active(state,
    super_index, XKB_STATE_MODS_DEPRESSED);

  xkb_mod_index_t shift_index = xkb_keymap_mod_get_index(
    keymap, XKB_MOD_NAME_SHIFT);

  int shift = xkb_state_mod_index_is_active(state,
    shift_index, XKB_STATE_MODS_DEPRESSED);

  xkb_mod_index_t ctrl_index = xkb_keymap_mod_get_index(
    keymap, XKB_MOD_NAME_CTRL);

  int ctrl = xkb_state_mod_index_is_active(state,
    ctrl_index, XKB_STATE_MODS_DEPRESSED);

  xkb_mod_index_t alt_index = xkb_keymap_mod_get_index(
    keymap, XKB_MOD_NAME_ALT);

  int alt = xkb_state_mod_index_is_active(state,
    alt_index, XKB_STATE_MODS_DEPRESSED);

  uint32_t keycode = event->keycode + 8;

  const xkb_keysym_t *syms;
  int nsyms = xkb_state_key_get_syms(state, keycode, &syms);

  if (event->state == 0) {
    for (int i = 0; i < nsyms; i++) {
      xkb_keysym_t sym = syms[i];
      if (sym == XKB_KEY_BackSpace) {
        if (ctrl && alt) {
          wl_display_terminate(keyboard->seat->server->wl_display);
        }
      }
      if (sym == XKB_KEY_Return) {
        if (super && !shift) {
          exec_command("gnome-terminal");
        }
        if (super && shift) {
          exec_command("weston-terminal");
        }
      }
      if (sym == XKB_KEY_F3) {
        exec_command("gnome-calendar");
      }
      if (sym == XKB_KEY_F4) {
        exec_command("gnome-calculator");
      }
      if (sym == XKB_KEY_F5) {
        exec_command("chrome");
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
