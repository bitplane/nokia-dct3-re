_G.sip_state_save_time = 27
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'sip_outgoing_state_roundtrip.lua')
