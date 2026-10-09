-- Physical 31 December 2026 fixture; firmware owns Gregorian arithmetic.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki6210_calendar_date_digits = {
    {4, 'Keypad 3'}, {2, 'Keypad 1'}, {2, 'Keypad 1'}, {3, 'Keypad 2'},
    {3, 'Keypad 2'}, {3, 'Keypad 0'}, {3, 'Keypad 2'}, {4, 'Keypad 6'}}
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_calendar_midnight_input.lua')
