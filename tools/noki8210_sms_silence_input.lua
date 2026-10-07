local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_sms_failure_observe_at = 39
_G.noki8210_sms_failure_observe_count = 75
_G.noki8210_sms_failure_observe_interval = 1
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_sms_reject_input.lua')
