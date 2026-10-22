-- The card installs its menu; enter/select/leave it through physical keys.
_G.nsb7_toolkit_menu=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_toolkit_interactive.lua')
