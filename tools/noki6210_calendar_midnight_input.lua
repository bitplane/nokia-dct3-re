-- Physical 23:59 entry followed by ordinary emulated time, not an RTC poke.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6210_calendar_time_digits = {
    {3, 'Keypad 2'}, {4, 'Keypad 3'}, {3, 'Keypad 5'}, {4, 'Keypad 9'}}
_G.noki6210_calendar_wait_midnight = true
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_calendar_input.lua')
