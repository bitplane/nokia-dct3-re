-- Physical Yes following restoration of the powered-off alarm countdown.
_G.noki6210_alarm_power_choice = 'yes'
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_power_restore.lua')
