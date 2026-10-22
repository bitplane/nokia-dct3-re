_G.nsb7_state_scenario='incoming_call'
_G.nsb7_state_observer='nsb7_pin_host_incoming_call.lua'
_G.nsb7_state_save_time=55
_G.nsb7_state_end_time=60
_G.nsb7_state_finish_time=68
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_state_roundtrip.lua')
