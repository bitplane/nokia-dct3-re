-- Physical 23:59/date entry followed by ordinary RTC-driven midnight.
-- Exploratory fixture: calendar-date advancement is not yet an acceptance claim.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_clock_settlement_only = true
_G.noki8890_clock_digits = {{3, 'Keypad 2'}, {4, 'Keypad 3'},
    {3, 'Keypad 5'}, {4, 'Keypad 9'}}
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_input.lua')
local input = coroutine.create(function()
    if not emu.wait(50) then return end
    manager.machine.screens[':screen']:snapshot('8890_midnight_before.png')
    if not emu.wait(50) then return end
    manager.machine.screens[':screen']:snapshot('8890_midnight_after.png')
end)
_G.noki8890_midnight_input = input
assert(coroutine.resume(input))
