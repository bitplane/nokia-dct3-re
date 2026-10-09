-- Set the alarm through physical input, then exit before its first match.
_G.noki6210_alarm_arm_only = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_input.lua')
