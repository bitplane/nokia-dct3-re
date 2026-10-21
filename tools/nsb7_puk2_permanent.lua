-- PIN2 exhaustion does not block ordinary PIN1 authentication or menus.
_G.nsb7_security_with_pin=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_menu_observe.lua')
