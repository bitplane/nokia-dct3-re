local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6250_state_sms = true
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_state_roundtrip.lua')
