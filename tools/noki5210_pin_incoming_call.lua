-- Delayed physical PIN followed by the laboratory incoming-call lifecycle.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki5210_pin_after_accept = function(press, snapshot)
    assert(emu.wait(10))
    snapshot('ringing')
    press('Send')
    assert(emu.wait(3))
    snapshot('connected')
    press('End')
    assert(emu.wait(5))
    snapshot('recovered')
end
dofile(directory .. 'noki5210_pin_late_input.lua')
