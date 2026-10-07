-- Save/load an organically connected outgoing call; resume physical End only.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_state_call = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_state_roundtrip.lua')
