-- Physical rail-off/alarm wake probe; the post-wake security editor is unresolved.
_G.noki8890_alarm_power_off = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_alarm_input.lua')
