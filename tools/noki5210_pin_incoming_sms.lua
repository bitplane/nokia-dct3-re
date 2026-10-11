-- Delayed physical PIN followed by organic SMS notification and Read.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki5210_pin_after_accept = function(press, snapshot)
    assert(emu.wait(10))
    snapshot('sms_notification')
    press('Left Softkey / Menu')
    assert(emu.wait(1.2))
    press('Left Softkey / Menu')
    assert(emu.wait(2.5))
    snapshot('sms_read')
end
dofile(directory .. 'noki5210_pin_late_input.lua')
