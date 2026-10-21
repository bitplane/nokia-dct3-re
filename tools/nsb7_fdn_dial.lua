_G.nsb7_security_with_pin=true
_G.nsb7_outgoing_call_delay=8
_G.nsb7_outgoing_call_digits=_G.nsb7_fdn_unlisted and '12347' or '12346'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_outgoing_call_observe.lua')
