-- Passive NSB-6 bootstrap access census; never supplies DSP values.
local source = debug.getinfo(1, "S").source:sub(2)
dofile(assert(source:match("^(.*[/])")) .. "dct3_model_scout.lua")
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local seen = {}
local tap = memory:install_read_tap(0x10000, 0x10103,
    "8890_bootstrap_read", function(address, data, mask)
        local pc = cpu.state["PC"].value
        if address > 0x10008 and address < 0x100fc then return end
        local key = string.format('%x:%x:%x', pc, address, mask)
        if seen[key] then return end
        seen[key] = true
        machine:logerror(string.format(
            "8890_bootstrap_read: pc=%08x address=%08x data=%08x mask=%08x t=%.6f\n",
            pc, address, data, mask, machine.time:as_double()))
    end)
_G.noki8890_bootstrap_tap = tap
