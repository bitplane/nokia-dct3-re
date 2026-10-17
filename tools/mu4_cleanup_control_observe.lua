-- Passive R060 AA99 data-bus evidence; context PC is not instruction ownership.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'cleanup observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local writes, reads = 0, 0
machine:logerror('mu4_cleanup_control_scope: address=aa1a boundary=data_bus context_pc=not_owner observer_writes=0 audio_acceptance=0\n')
local write = data:install_write_tap(0xaa1a, 0xaa1a, 'mu4_cleanup_control_write', function(_, value, mask)
    writes = writes + 1
    if writes <= 32 then
        machine:logerror(string.format(
            'mu4_cleanup_control_write: ordinal=%u value=%04x mask=%04x t=%.9f context_pc=%06x observer_writes=0\n',
            writes, value, mask, machine.time:as_double(), cpu.state.GENPC.value))
    end
end)
local read = data:install_read_tap(0xaa1a, 0xaa1a, 'mu4_cleanup_control_read', function(_, value)
    reads = reads + 1
    if reads <= 32 then
        machine:logerror(string.format(
            'mu4_cleanup_control_read: ordinal=%u value=%04x t=%.9f context_pc=%06x observer_writes=0\n',
            reads, value, machine.time:as_double(), cpu.state.GENPC.value))
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format(
        'mu4_cleanup_control_terminal: writes=%u reads=%u capped=%u illegal=%u t=%.9f observer_writes=0 audio_acceptance=0\n',
        writes, reads, (writes > 32 or reads > 32) and 1 or 0, cpu.state.ILLEGAL.value,
        machine.time:as_double()))
end)
_G.mu4_cleanup_control_handles = {write, read, stop}
