-- Hold before physical Answer; use the same exact architecture restoration.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_incoming_alerting_hold = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_sip_incoming_restore.lua')
