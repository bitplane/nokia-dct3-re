-- Compare explicit physical Power-on with the board's automatic power latch.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki8850_startup_observe.lua")
local machine = manager.machine
local power = assert(machine.ioport.ports[":PWR"].fields["Power"])
power:set_value(1)
local release = coroutine.create(function()
    if not emu.wait(1.5) then power:set_value(0); return end
    power:set_value(0)
    machine:logerror("8850_power_start: released t=1.5\n")
end)
_G.noki8850_power_release = release
assert(coroutine.resume(release))
