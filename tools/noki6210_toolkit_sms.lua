-- Wait for card-owned SMS completion before physical return to idle.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki6210_toolkit_network_wait = 20
dofile(directory .. 'noki6210_toolkit_menu.lua')
