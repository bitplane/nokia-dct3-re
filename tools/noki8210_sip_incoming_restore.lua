-- Physical incoming Answer, exact handset architecture restore, no SIP replay.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8210_state_incoming = true
_G.noki8210_host_incoming = true
dofile(directory .. 'noki8210_state_roundtrip.lua')
dofile(directory .. 'noki8210_sip_restore_observe.lua')
