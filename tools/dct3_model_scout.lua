-- Bounded observation, not acceptance: never writes firmware or reads MMIO.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local screen = assert(machine.screens[":screen"])
local samples = coroutine.create(function()
    local previous = 0
    for _, t in ipairs({0.1, 0.5, 1, 2, 4, 8}) do
        if not emu.wait(t - previous) then return end
        previous = t
        machine:logerror(string.format("model_scout: t=%.3f pc=%08x\n",
            machine.time:as_double(), cpu.state["PC"].value))
        screen:snapshot(string.format("scout_%04d.png", t * 1000))
    end
end)
assert(coroutine.resume(samples))
