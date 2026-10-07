local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8850_state_sms = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8850_state_roundtrip.lua')
