_G.nsb7_security_with_pin=true
_G.nsb7_state_scenario='incoming_call'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_state_roundtrip.lua')
