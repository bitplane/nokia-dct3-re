-- R060 playback bench: raw data-write evidence, not Execute ownership or audio acceptance.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'playback lifecycle observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles, counts, last = {}, {}, {}
local control_reads, cleanup_reads, cleanup_samples = 0, 0, 0
local fields = {0x373e, 0x3742, 0x3750, 0x3762}
machine:logerror('mu4_playback_lifecycle_scope: boundary=data_write context_pc=not_owner observer_writes=0 audio_acceptance=0\n')
for index, field in ipairs(fields) do
    counts[field] = 0
    handles[index] = data:install_write_tap(field, field, 'mu4_playback_lifecycle_' .. field,
        function(_, value, mask)
            counts[field] = counts[field] + 1
            last[field] = value
            if counts[field] <= 16 then
                machine:logerror(string.format(
                    'mu4_playback_lifecycle_write: address=%04x value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x observer_writes=0\n',
                    field, value, mask, counts[field], machine.time:as_double(), cpu.state.GENPC.value))
            end
        end)
end
handles[#handles + 1] = data:install_read_tap(0x3742, 0x3742, 'mu4_playback_control_read', function(_, value)
    control_reads = control_reads + 1
    local pc = cpu.state.GENPC.value
    if pc >= 0x29560 and pc <= 0x29590 then
        cleanup_reads = cleanup_reads + 1
        if cleanup_samples < 4 then
            cleanup_samples = cleanup_samples + 1
            machine:logerror(string.format(
                'mu4_playback_control_read: value=%04x t=%.9f context_pc=%06x selection=context_range reader_identity=unvalidated observer_writes=0\n',
                value, machine.time:as_double(), pc))
        end
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format(
        'mu4_playback_lifecycle_terminal: pc=%06x illegal=%u lifecycle_writes=%u control_writes=%u control_last=%04x control_reads=%u cleanup_context_reads=%u mode_writes=%u continuation_writes=%u observer_writes=0 audio_acceptance=0\n',
        cpu.state.GENPC.value, cpu.state.ILLEGAL.value, counts[0x373e], counts[0x3742], last[0x3742] or 0,
        control_reads, cleanup_reads, counts[0x3750], counts[0x3762]))
end)
_G.mu4_playback_lifecycle_handles = {handles, stop}
