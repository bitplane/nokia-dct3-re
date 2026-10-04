-- Passive ROM4 reference: status publication is distinct from identity flags.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local data = cpu.spaces["data"]
local writes = 0
local tap = data:install_write_tap(0x06f9, 0x06f9,
    "nse1_selftest_status", function(offset, value, mask)
        writes = writes + 1
        machine:logerror(string.format(
            "nse1_selftest_status_write: pc=%04x value=%04x mask=%04x t=%.6f\n",
            cpu.state["PC"].value, value, mask, machine.time:as_double()))
    end)
local checked = false
emu.register_periodic(function()
    if checked or machine.time:as_double() < 3 then return end
    checked = true
    machine:logerror(string.format(
        "nse1_selftest_status_summary: status=%04x identity_flags=%04x writes=%d completion=%04x t=%.6f\n",
        data:read_u16(0x06f9), data:read_u16(0x1f11), writes,
        data:read_u16(0x0880), machine.time:as_double()))
end)
-- Keep the observer installed for the entire run.
_G.nse1_selftest_status_tap = tap
