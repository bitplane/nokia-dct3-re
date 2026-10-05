-- Passive observation of the own-ROM primitive-35 branch selector.
local source = debug.getinfo(1, "S").source:sub(2)
dofile(assert(source:match("^(.*[/])")) .. "nsm3d_verifier_observe.lua")
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local count = 0
local tap = memory:install_write_tap(0x13fdd8, 0x13fddb,
    "nsm3d_record_selector", function(address, data, mask)
        if count >= 40 then return end
        count = count + 1
        machine:logerror(string.format(
            "8250_record_selector_write: pc=%08x address=%08x data=%08x mask=%08x t=%.6f\n",
            cpu.state["PC"].value, address, data, mask, machine.time:as_double()))
    end)
_G.noki8250_record_context_tap = tap
local observe = coroutine.create(function()
    if not emu.wait(8) then return end
    machine:logerror(string.format("8250_record_selector: address=0013fdd9 value=%02x\n",
        memory:read_u8(0x13fdd9)))
end)
_G.noki8250_record_context_observer = observe
assert(coroutine.resume(observe))
