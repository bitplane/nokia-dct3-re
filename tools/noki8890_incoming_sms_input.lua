-- Network-originated SMS, physical UI input only.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8890_security_only = true
dofile(directory .. 'noki8890_security_input.lua')
dofile(directory .. 'noki8890_sms_read.lua')
