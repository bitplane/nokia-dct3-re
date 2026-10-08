-- Cold-restart observation after physical security-code entry only.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_security_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    for _, when in ipairs({12, 21, 30, 90}) do
        local delay = when - machine.time:as_double()
        if delay > 0 and not emu.wait(delay) then return end
        machine.screens[':screen']:snapshot('8890_clock_cold_' .. when .. '.png')
        machine:logerror('8890_clock_cold: t=' .. when .. '\n')
    end
end)
_G.noki8890_clock_read = input
assert(coroutine.resume(input))
