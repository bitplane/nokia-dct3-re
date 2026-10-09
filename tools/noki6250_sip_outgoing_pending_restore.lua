-- Hold the physically dialed call until external SIP invalidation clears it.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6250_sip_restore_outgoing = true
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_sip_call_restore.lua')
