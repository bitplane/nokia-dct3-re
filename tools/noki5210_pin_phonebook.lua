local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki5210_pin_after_accept = function(press, snapshot)
    assert(emu.wait(10))
    snapshot('idle')
    _G.noki5210_pin_duration = 0.18
    _G.noki5210_pin_gap = 0.26
    press('Right Softkey / C')
    assert(emu.wait(0.7))
    if _G.noki5210_pin_phonebook_read then
        press('Left Softkey / Menu')
        assert(emu.wait(0.7))
        press('Left Softkey / Menu')
        assert(emu.wait(0.7))
        snapshot('phonebook_list')
        press('Left Softkey / Menu')
    else
        press('Scroll Down')
        assert(emu.wait(0.4))
        press('Left Softkey / Menu')
        assert(emu.wait(0.7))
        for digit in ('232'):gmatch('.') do press('Keypad ' .. digit) end
        assert(emu.wait(1.2))
        press('Left Softkey / Menu')
        assert(emu.wait(0.8))
        for digit in ('123'):gmatch('.') do press('Keypad ' .. digit) end
        assert(emu.wait(0.8))
        press('Left Softkey / Menu')
    end
    assert(emu.wait(2.5))
    snapshot('phonebook')
end
dofile(assert(source:match('^(.*[/])')) .. 'noki5210_pin_late_input.lua')
