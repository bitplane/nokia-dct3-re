-- Physical Cancel of the card-owned call consent prompt.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki6210_toolkit_decline = true
dofile(directory .. 'noki6210_toolkit_call.lua')
