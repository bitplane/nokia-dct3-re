-- Restore only an idle handset, before creating any external SIP dialog.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki6210_state_scenario = 'idle'
_G.noki6210_sip_restore_idle = true
dofile(directory .. 'noki6210_state_roundtrip.lua')
dofile(directory .. 'noki6210_sip_cancel_observe.lua')
