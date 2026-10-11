-- Clock settlement, physical Answer, then architectural save/load.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_sip_restore_connected = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_sip_call_restore.lua')
