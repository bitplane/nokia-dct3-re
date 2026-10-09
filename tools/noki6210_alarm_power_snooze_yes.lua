-- Own NPE-3 physical Yes after a powered-off Snooze recurrence.
_G.noki6210_alarm_snooze = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_power_yes.lua')
