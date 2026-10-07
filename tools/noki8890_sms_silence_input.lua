-- Capture the handset-owned RP timeout, not an injected release.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_sms_failure_observe_at = 40
_G.noki8890_sms_failure_observe_count = 70
_G.noki8890_sms_failure_observe_interval = 1
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_sms_reject_input.lua')
