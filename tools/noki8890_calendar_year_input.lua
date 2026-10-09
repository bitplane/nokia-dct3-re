-- Physical year-end entry; firmware owns month/year rollover and persistence.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_date_digits = {{4, 'Keypad 3'}, {2, 'Keypad 1'},
    {2, 'Keypad 1'}, {3, 'Keypad 2'}, {3, 'Keypad 2'},
    {3, 'Keypad 0'}, {3, 'Keypad 2'}, {4, 'Keypad 6'}}
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_calendar_rollover_input.lua')
