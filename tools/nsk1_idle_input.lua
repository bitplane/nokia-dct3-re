-- Cold authentication only; no repeat of the diagnostic-display selection.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
_G.nsk1_security_code_only = true
dofile(directory .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local actions = coroutine.create(function()
    for _, time in ipairs({25,36,42}) do
        assert(emu.wait(time-machine.time:as_double()))
        machine.screens[':screen']:snapshot(string.format('nsk1_idle_%02d.png',time))
    end
end)
assert(coroutine.resume(actions))
