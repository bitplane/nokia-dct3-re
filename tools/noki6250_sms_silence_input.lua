local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6250_sms_silence = true
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_sms_reject_input.lua')
