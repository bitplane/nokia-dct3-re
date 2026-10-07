_G.sip_state_scenario = 'outgoing'
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'sip_call_state_roundtrip.lua')
