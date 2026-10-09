-- Card-menu selection uses the independently observed NSB-6 input contract.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki8890_toolkit_menu = true
dofile(directory .. 'noki8890_toolkit_interactive_input.lua')
