project('compositor', 'c', default_options : [
  'warning_level=3',
	'werror=true',
  'c_std=gnu11'
])

wlroots = dependency('wlroots')
wayland = dependency('wayland-server')
xkbcommon = dependency('xkbcommon')

include_directories = include_directories('include', '/usr/include/pixman-1')

executable('compositor',
  'src/main.c',
  'src/wm_keyboard.c',
  'src/wm_output.c',
  'src/wm_pointer.c',
  'src/wm_seat.c',
  'src/wm_server.c',
  'src/wm_surface.c',
   'src/wm_window.c',
  include_directories: include_directories,
  dependencies: [wlroots, wayland, xkbcommon]
)
