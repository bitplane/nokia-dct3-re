_G.nsb7_security_with_pin=true
_G.nsb7_sms_read_times={23,25,27,29}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_sms_read_observe.lua')
