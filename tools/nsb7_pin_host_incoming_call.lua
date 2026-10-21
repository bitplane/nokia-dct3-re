_G.nsb7_security_with_pin=true
_G.nsb7_call_times={52,60}
_G.nsb7_call_finish_time=68
_G.nsb7_call_ready_time=45
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_call_observe.lua')
