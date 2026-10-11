-- Physical Answer then save/load; the external dialog is cleared, not replayed.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6250_sip_restore_connected = true
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_sip_call_restore.lua')
