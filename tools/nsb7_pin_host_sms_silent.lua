_G.nsb7_sms_recovery_delay=90
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_host_sms_failure.lua')
