-- Own NPE-3 physical Snooze after an autonomous powered-off alarm wake.
_G.noki6210_alarm_snooze = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_power_no.lua')
