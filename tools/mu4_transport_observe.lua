-- Passive, recording-specific transport interval; GENPC is context only.
-- The preset brackets the first fixed-offset disagreement and following reset.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'transport observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local first, last = 9.698, 9.876
local selected, writes, configs = nil, 0, 0
local handles = {}
handles[1] = data:install_write_tap(0x38, 0x39, 'transport_config', function(address, value, mask)
    if machine:side_effects_disabled() then return end
    if address == 0x38 then selected = value & 0x1f end
    configs = configs + 1
    machine:logerror(string.format('mu4_transport_config: address=%04x index=%s value=%04x mask=%04x t=%.9f context_pc=%06x observer_writes=0\n',
        address, selected and string.format('%02x', selected) or 'unknown', value, mask,
        machine.time:as_double(), cpu.state.GENPC.value))
end)
handles[2] = data:install_write_tap(0x23, 0x23, 'transport_dxr', function(address, value, mask)
    if machine:side_effects_disabled() then return end
    local time = machine.time:as_double()
    if time < first or time > last then return end
    writes = writes + 1
    machine:logerror(string.format('mu4_transport_dxr: ordinal=%u value=%04x mask=%04x t=%.9f context_pc=%06x observer_writes=0\n',
        writes, value, mask, time, cpu.state.GENPC.value))
end)
handles[3] = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('mu4_transport_terminal: configs=%u dxr=%u first=%.9f last=%.9f t=%.9f observer_writes=0\n',
        configs, writes, first, last, machine.time:as_double()))
end)
_G.mu4_transport_observation_handles = handles
