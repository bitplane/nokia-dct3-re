-- Consent uses only the handset's physical softkey.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki6250_toolkit_call = true
_G.noki6250_toolkit_network_wait = 20
dofile(directory .. 'noki6250_toolkit_menu.lua')
