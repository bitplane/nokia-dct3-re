-- Existing physical call and exact architectural save/load; never restore SIP.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki8210_state_call.lua')
dofile(directory .. 'noki8210_sip_restore_observe.lua')
