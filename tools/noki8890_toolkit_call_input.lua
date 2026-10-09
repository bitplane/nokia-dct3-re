-- Physical consent to the card's SET UP CALL request.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki8890_toolkit_call = true
_G.noki8890_toolkit_network_wait = 20
dofile(directory .. 'noki8890_toolkit_menu_input.lua')
