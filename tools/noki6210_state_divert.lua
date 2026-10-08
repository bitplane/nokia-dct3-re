_G.noki6210_state_scenario = 'divert'
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_state_roundtrip.lua')
