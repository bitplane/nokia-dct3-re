_G.nsb3_security_base = 'nsb3_pin_disabled.lua'
_G.nsb3_security_enable = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_security_input.lua')
