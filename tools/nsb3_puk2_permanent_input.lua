-- Select PIN2 after permanent loss, then navigate away using physical Down.
_G.nsb3_change_pin2 = true
_G.nsb3_change_digits = {}
_G.nsb3_change_extra_steps = {{80, 'Scroll Down'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_change_input.lua')
local settled = coroutine.create(function()
    assert(emu.wait(90))
    manager.machine.screens[':screen']:snapshot('nsb3_puk2_permanent_settled.png')
end)
assert(coroutine.resume(settled))
