-- Card installs the menu; all navigation remains physical.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki6210_toolkit_menu = true
dofile(directory .. 'noki6210_toolkit_interactive.lua')
