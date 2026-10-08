-- Physical 28 February 2024 entry; the firmware owns leap-year arithmetic.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_date_digits = {{3, 'Keypad 2'}, {3, 'Keypad 8'},
    {3, 'Keypad 0'}, {3, 'Keypad 2'}, {3, 'Keypad 2'},
    {3, 'Keypad 0'}, {3, 'Keypad 2'}, {2, 'Keypad 4'}}
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_calendar_rollover_input.lua')
