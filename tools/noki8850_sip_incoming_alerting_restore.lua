-- Withhold Answer: restoration must reject, not connect, the external invite.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8850_incoming_alerting_hold = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8850_sip_incoming_restore.lua')
