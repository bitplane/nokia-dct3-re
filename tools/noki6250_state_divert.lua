_G.noki6250_state_divert = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_state_roundtrip.lua')
