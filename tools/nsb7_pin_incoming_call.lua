_G.nsb7_security_with_pin=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_call_observe.lua')
