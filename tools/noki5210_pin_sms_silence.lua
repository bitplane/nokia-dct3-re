-- Observe the firmware-owned timeout; only physical input changes handset state.
local source = debug.getinfo(1, 'S').source:sub(2)
local function finish(press, snapshot, delay)
    assert(emu.wait(delay))
    for index = 1, 25 do
        snapshot('sms_timeout_' .. index)
        assert(emu.wait(1))
    end
    press('End')
    assert(emu.wait(4))
    snapshot('recovered')
    press('Left Softkey / Menu')
    assert(emu.wait(2))
    snapshot('sms_recovery_menu')
end
_G.noki5210_pin_sms_after_submit = function(press, snapshot)
    if _G.noki5210_pin_sms_save_wait then
        _G.noki5210_pin_sms_save_wait(press, snapshot, finish)
    else
        finish(press, snapshot, 60)
    end
end
dofile(assert(source:match('^(.*[/])')) .. 'noki5210_pin_outgoing_sms.lua')
