-- Allow the card-requested SMS transaction to finish before physical End.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki8850_toolkit_network_wait = 20
dofile(directory .. 'noki8850_toolkit_menu_input.lua')
