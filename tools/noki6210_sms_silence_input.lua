local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6210_sms_silence = true
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_sms_reject_input.lua')
