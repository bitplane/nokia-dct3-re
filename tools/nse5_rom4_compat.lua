-- Observation-only: this composition tests compatibility, not fitted silicon.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local space = cpu.spaces.program
local next_sample = 1
local times = {0.1, 0.5, 1, 2, 4, 8}
emu.register_frame_done(function()
    if next_sample > #times or machine.time:as_double() < times[next_sample] then return end
    machine.screens[":screen"]:snapshot(string.format("native_%02d.png", next_sample))
    machine:logerror(string.format(
        "nse5_compat_sample: t=%.6f pc=%08x result0=%04x result1=%04x idle=%02x\n",
        machine.time:as_double(), cpu.state["PC"].value,
        space:read_u16(0x167036), space:read_u16(0x167038), space:read_u8(0x168f04)))
    next_sample = next_sample + 1
end, "NSE-5 ROM4 compatibility observation")
