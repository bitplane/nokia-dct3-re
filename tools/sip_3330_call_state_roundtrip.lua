-- NHM-6 answers near 11.5 s; save/load must precede the remote's 8 s hangup.
_G.sip_state_save_time = 17
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'sip_call_state_roundtrip.lua')
