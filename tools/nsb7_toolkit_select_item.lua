-- Select the second item in the proactive dialog using physical contacts.
_G.nsb7_toolkit_menu=true
_G.nsb7_toolkit_select_item=true
_G.nsb7_toolkit_network_wait=10
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_toolkit_interactive.lua')
