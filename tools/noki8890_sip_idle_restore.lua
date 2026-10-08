-- Restore idle architectural state before admitting a fresh external dialog.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8890_sip_restore_idle = true
dofile(directory .. 'noki8890_state_roundtrip.lua')
dofile(directory .. 'noki8890_sip_cancel_observe.lua')
