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
    wlr_log(WLR_ERROR, "cannot execute binding command: fork() failed");
    return;
  } else if (pid == 0) {
    execl("/bin/sh", "-c", shell_cmd, (void *)NULL);
  }
}

static bool mod_state(const char* mod_name, struct xkb_keymap* keymap, struct xkb_state* state) {
  xkb_mod_index_t index = xkb_keymap_mod_get_index(keymap, mod_name);

  int active = xkb_state_mod_index_is_active(state,
    index, XKB_STATE_MODS_DEPRESSED);

  return active;
}

void wm_keyboard_modifiers_event(struct wm_keyboard *keyboard) {
  wlr_seat_keyboard_notify_modifiers(keyboard->seat->seat,
    &keyboard->device->keyboard->modifiers);

  struct xkb_state* state = keyboard->device->keyboard->xkb_state;
  struct xkb_keymap* keymap = keyboard->device->keyboard->keymap;

  int super = mod_state(XKB_MOD_NAME_LOGO, keymap, state);
  int alt = mod_state(XKB_MOD_NAME_ALT, keymap, state);

  if (!super && !alt) {
    wm_server_commit_window_switch(keyboard->seat->server, keyboard->seat);
  }
}

void wm_keyboard_key_event(struct wm_keyboard *keyboard,
  struct wlr_event_keyboard_key *event) {

  struct xkb_state* state = keyboard->device->keyboard->xkb_state;
  struct xkb_keymap* keymap = keyboard->device->keyboard->keymap;

  int super = mod_state(XKB_MOD_NAME_LOGO, keymap, state);
  int shift = mod_state(XKB_MOD_NAME_SHIFT, keymap, state);
  int ctrl = mod_state(XKB_MOD_NAME_CTRL, keymap, state);
  int alt = mod_state(XKB_MOD_NAME_ALT, keymap, state);

  uint32_t keycode = event->keycode + 8;

  const xkb_keysym_t *syms;
  int nsyms = xkb_state_key_get_syms(state, keycode, &syms);

  if (event->state == WLR_KEY_PRESSED) {
    for (int i = 0; i < nsyms; i++) {
      xkb_keysym_t sym = syms[i];

      if (super && sym == XKB_KEY_Up) {
        wm_server_maximize_focused_window(keyboard->seat->server);
        return;
      }

      if (super && sym == XKB_KEY_k) {
        wm_server_switch_window(keyboard->seat->server);
        return;
      }

      if (super && sym == XKB_KEY_b) {
        exec_command("chrome");
        return;
      }

      if (super && sym == XKB_KEY_s) {
        exec_command("slack-desktop");
        return;
      }

      if (sym == XKB_KEY_F1) {
        exec_command("chrome");
        return;
      }

      if (super && sym == XKB_KEY_space) {
        exec_command("dmenu_run");
        return;
      }

      if (alt && sym == XKB_KEY_Tab) {
        wm_server_switch_window(keyboard->seat->server);
        return;
      }

      if (super && sym == XKB_KEY_Tab) {
        wm_server_switch_window(keyboard->seat->server);
        return;
      }
    }
  }

  if (event->state == WLR_KEY_RELEASED) {
    for (int i = 0; i < nsyms; i++) {
      xkb_keysym_t sym = syms[i];
      if (sym == XKB_KEY_BackSpace) {
        if (ctrl && alt) {
          wl_display_terminate(keyboard->seat->server->wl_display);
          return;
        }
      }

      printf("%d %d %d\n", super, shift, sym);

      if (sym == XKB_KEY_Return) {
        if (super && !shift) {
          exec_command("gnome-terminal");
          return;
        }
        if (super && shift) {
          exec_command("weston-terminal");
          return;
        }
      }
    }
  }

  wlr_seat_keyboard_notify_key(
    keyboard->seat->seat,
    event->time_msec,
    event->keycode,
    event->state
  );
}

static void keyboard_modifiers_notify(struct wl_listener *listener, void *data) {
  (void)data;
  struct wm_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
  wm_keyboard_modifiers_event(keyboard);
}

static void keyboard_key_notify(struct wl_listener *listener, void *data) {
  struct wlr_event_keyboard_key *event = data;
  struct wm_keyboard *keyboard = wl_container_of(listener, keyboard, key);
  wm_keyboard_key_event(keyboard, event);
}

struct wm_keyboard* wm_keyboard_create(struct wlr_input_device* device,
  struct wm_seat* seat) {
  struct wm_keyboard *keyboard = calloc(1, sizeof(struct wm_keyboard));
  keyboard->device = device;
  keyboard->seat = seat;

  wl_list_insert(&seat->keyboards, &keyboard->link);

  int repeat_rate = 25;
  int repeat_delay = 600;
  wlr_keyboard_set_repeat_info(device->keyboard, repeat_rate, repeat_delay);

  wl_signal_add(&device->keyboard->events.key, &keyboard->key);
  keyboard->key.notify = keyboard_key_notify;

  wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
  keyboard->modifiers.notify = keyboard_modifiers_notify;

  wlr_seat_set_keyboard(seat->seat, device);

  struct xkb_rule_names rules = { 0 };
  rules.rules = getenv("XKB_DEFAULT_RULES");
  rules.model = getenv("XKB_DEFAULT_MODEL");
  rules.layout = getenv("XKB_DEFAULT_LAYOUT");
  rules.variant = getenv("XKB_DEFAULT_VARIANT");
  rules.options = getenv("XKB_DEFAULT_OPTIONS");

  struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  if (!context) {
    wlr_log(WLR_ERROR, "Failed to create XKB context");
    exit(1);
  }

  struct xkb_keymap *keymap = xkb_map_new_from_names(context,
    &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

  wlr_keyboard_set_keymap(device->keyboard, keymap);

  xkb_context_unref(context);
  xkb_keymap_unref(keymap);

  return keyboard;
}
