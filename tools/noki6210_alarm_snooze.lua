-- Physical Snooze observation using the same independently provisioned clock.
_G.noki6210_alarm_snooze = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_input.lua')
