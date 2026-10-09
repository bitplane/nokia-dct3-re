-- Own physical dialing; save/load clears an unanswered external SIP request.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_sip_restore_outgoing = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_sip_call_restore.lua')
