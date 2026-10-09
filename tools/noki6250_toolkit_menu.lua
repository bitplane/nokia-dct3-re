-- Own NHM-3 physical route to a card-installed menu.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki6250_toolkit_menu = true
dofile(directory .. 'noki6250_toolkit_interactive.lua')
