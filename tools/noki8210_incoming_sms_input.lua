-- Laboratory SMS delivered over GSM; physical Read keys only.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_security_input.lua')
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_sms_read.lua')
