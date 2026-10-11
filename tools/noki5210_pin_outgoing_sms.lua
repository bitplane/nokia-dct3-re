-- Own physical predictive-text editor route after delayed PIN registration.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki5210_pin_after_accept = function(press, snapshot)
    assert(emu.wait(10))
    _G.noki5210_pin_duration = 0.5
    _G.noki5210_pin_gap = 0.5
    for _ = 1, 3 do
        press('Left Softkey / Menu')
        assert(emu.wait(1))
    end
    press('Keypad 2')
    assert(emu.wait(2))
    snapshot('sms_composed')
    press('Left Softkey / Menu')
    assert(emu.wait(2))
    press('Left Softkey / Menu')
    assert(emu.wait(2))
    for digit in ('5551234'):gmatch('.') do press('Keypad ' .. digit) end
    assert(emu.wait(1))
    press('Left Softkey / Menu')
    if _G.noki5210_pin_sms_after_submit then
        _G.noki5210_pin_sms_after_submit(press, snapshot)
        return
    end
    assert(emu.wait(1.2))
    snapshot('sms_sent')
end
dofile(directory .. 'noki5210_pin_late_input.lua')
