_G.nsb7_state_scenario='call'
_G.nsb7_state_observer='nsb7_pin_host_outgoing_call.lua'
_G.nsb7_state_save_time=60
_G.nsb7_state_end_time=66
_G.nsb7_state_finish_time=76
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_state_roundtrip.lua')
