-- Physical dismissal/re-entry control after permanent PIN2 rejection.
_G.nsb3_change_pin2 = true
_G.nsb3_change_digits = {}
_G.nsb3_change_extra_steps = {
    {80, 'Right Softkey / C'}, {82, 'End'}, {85, 'Left Softkey / Menu'}}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_change_input.lua')
local settled = coroutine.create(function()
    assert(emu.wait(90))
    manager.machine.screens[':screen']:snapshot('nsb3_puk2_dismiss_settled.png')
end)
assert(coroutine.resume(settled))
