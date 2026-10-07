-- The 3330 connects near 12.5 s; save before the remote eight-second hangup.
_G.sip_state_save_time = 17
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'sip_outgoing_state_roundtrip.lua')
