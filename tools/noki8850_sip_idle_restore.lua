-- Restore idle before admitting a new external SIP dialog.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8850_state_idle = true
_G.noki8850_sip_restore_idle = true
dofile(directory .. 'noki8850_state_roundtrip.lua')
dofile(directory .. 'noki8850_sip_cancel_observe.lua')
