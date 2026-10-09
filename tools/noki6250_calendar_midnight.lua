-- Bounded own-product rollover probe; time is entered through physical digits.
_G.noki6250_calendar_time = {'2', '3', '5', '9'}
_G.noki6250_calendar_midnight = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_calendar_input.lua')
