-- Physical supplementary-service request after own PIN-authenticated boot.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.noki5210_pin_after_accept = function(press, snapshot)
    assert(emu.wait(10))
    snapshot('idle')
    _G.noki5210_pin_duration = 0.22
    _G.noki5210_pin_gap = 0.28
    for _, name in ipairs({'Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #'}) do
        press(name)
    end
    assert(emu.wait(0.8))
    press('Send')
    assert(emu.wait(6))
    snapshot('ussd')
    press('End')
    assert(emu.wait(3))
    snapshot('recovered')
end
dofile(directory .. 'noki5210_pin_late_input.lua')
