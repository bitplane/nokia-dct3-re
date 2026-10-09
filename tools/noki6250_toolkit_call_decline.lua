-- Refuse the card-requested call without altering firmware state.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki6250_toolkit_call_decline = true
dofile(directory .. 'noki6250_toolkit_call.lua')
