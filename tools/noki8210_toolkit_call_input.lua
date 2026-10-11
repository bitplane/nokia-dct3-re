-- The card requests the call; physical Menu supplies user consent.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki8210_toolkit_call = true
_G.noki8210_toolkit_network_wait = 20
dofile(directory .. 'noki8210_toolkit_menu_input.lua')
