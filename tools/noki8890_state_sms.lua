-- Save/load an organically delivered SMS, then resume physical reading only.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_state_sms = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_state_roundtrip.lua')
