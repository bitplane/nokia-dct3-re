-- Physical alarm entry, shutdown and natural RTC wake observation.
_G.noki6210_alarm_power_off = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_input.lua')
