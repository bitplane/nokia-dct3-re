-- Physical No at the post-alarm activation prompt.
_G.noki6210_alarm_power_choice = 'no'
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_power_off.lua')
