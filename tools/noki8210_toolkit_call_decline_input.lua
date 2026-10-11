-- Physical rejection must prevent the card-owned request starting a call.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki8210_toolkit_decline = true
dofile(directory .. 'noki8210_toolkit_call_input.lua')
