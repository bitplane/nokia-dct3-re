-- Passive CLKMD audit. Start with -autoboot_delay 0; never writes CPU or memory state.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'MU4 clock observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local handles, writes, resets = {}, 0, 0
handles[1] = cpu.spaces.data:install_write_tap(0x58, 0x58, 'mu4_clkmd', function(_, value, mask)
    writes = writes + 1
    machine:logerror(string.format('mu4_clock_write: pc=%06x value=%04x mask=%04x previous=%04x t=%.9f\n',
        cpu.state['GENPC'].value, value, mask, cpu.spaces.data:read_u16(0x58), machine.time:as_double()))
end)
local function snapshot(phase)
    machine:logerror(string.format('mu4_clock_snapshot: phase=%s resets=%u clkmd=%04x t=%.9f\n',
        phase, resets, cpu.spaces.data:read_u16(0x58), machine.time:as_double()))
end
snapshot('initial')
local reset = emu.add_machine_reset_notifier(function()
    resets = resets + 1
    snapshot('reset')
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_clock_summary: resets=%u writes=%u clkmd=%04x t=%.9f\n',
        resets, writes, cpu.spaces.data:read_u16(0x58), machine.time:as_double()))
end)
_G.mu4_clock_handles = {handles = handles, reset = reset, stop = stop}
