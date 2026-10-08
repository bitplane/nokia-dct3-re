-- Restore handset idle before admitting a fresh external SIP dialog.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8210_sip_restore_idle = true
dofile(directory .. 'noki8210_state_roundtrip.lua')
dofile(directory .. 'noki8210_sip_cancel_observe.lua')
