-- Restore handset idle before admitting a new external dialog.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki6250_sip_restore_idle = true
-- A pre-load coroutine wait is cancelled by MAME's state load.
_G.noki6250_begin_fresh_sip = function()
    dofile(directory .. 'noki6250_sip_cancel_observe.lua')
end
dofile(directory .. 'noki6250_state_roundtrip.lua')
