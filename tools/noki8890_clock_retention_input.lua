-- Distinct physical 13:47 fixture; never write RTC/MMI state directly.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_clock_settlement_only = true
_G.noki8890_clock_digits = {{2, 'Keypad 1'}, {4, 'Keypad 3'},
    {2, 'Keypad 4'}, {2, 'Keypad 7'}}
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_input.lua')
