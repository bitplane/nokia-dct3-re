_G.nsb7_security_with_pin=true
_G.nsb7_outgoing_call_delay=8
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_outgoing_call_observe.lua')
