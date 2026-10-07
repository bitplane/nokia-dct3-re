-- Observe the firmware-owned timeout before attempting physical recovery.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8850_sms_failure_observe_at = 38
_G.noki8850_sms_failure_observe_count = 75
_G.noki8850_sms_failure_observe_interval = 1
dofile(assert(source:match('^(.*[/])')) .. 'noki8850_sms_reject_input.lua')
