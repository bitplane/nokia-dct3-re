-- Physical dialling after coherent delayed PIN; no firmware-state writes.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki5210_pin_after_accept = function(press, snapshot)
    assert(emu.wait(10))
    _G.noki5210_pin_gap = 0.35
    snapshot('idle')
    for digit in ('5551234'):gmatch('.') do press('Keypad ' .. digit) end
    press('Send')
    assert(emu.wait(6))
    snapshot('connected')
    press('End')
    assert(emu.wait(8))
    snapshot('recovered')
end
dofile(directory .. 'noki5210_pin_late_input.lua')
