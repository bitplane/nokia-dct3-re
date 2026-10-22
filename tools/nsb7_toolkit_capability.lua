-- Physical menu selection followed by a bounded capability observation.
_G.nsb7_toolkit_menu=true
_G.nsb7_toolkit_network_wait=20
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_toolkit_interactive.lua')
