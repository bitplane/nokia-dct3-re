_G.nsb7_security_with_pin=true
_G.nsb7_state_scenario='sms'
-- Retained-clock CP-ACK22.024441 precedes RP-ACK22.040198.
_G.nsb7_state_save_time=22.03
_G.nsb7_sms_read_times={26,28,30,32}
_G.nsb7_state_finish_time=35
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_state_roundtrip.lua')
