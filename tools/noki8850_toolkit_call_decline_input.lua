-- Decline the card-owned request using the handset's physical Cancel key.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki8850_toolkit_decline = true
dofile(directory .. 'noki8850_toolkit_call_input.lua')
