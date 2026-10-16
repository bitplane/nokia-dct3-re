-- Fixed-time RAM context only: no writer identity, instruction operands or audio claim.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'MU4 playback context requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles, read_counts, latch_writes, sampling = {}, {}, 0, false
handles[1] = data:install_read_tap(0x804a, 0x804f, 'mu4_playback_descriptor_reads', function(address, value)
    local time = machine.time:as_double()
    if sampling or time < 0.8 or time > 2.4 then return end
    read_counts[address] = (read_counts[address] or 0) + 1
    if read_counts[address] <= 8 then
        machine:logerror(string.format(
            'mu4_playback_descriptor_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
            address, value, read_counts[address], time, cpu.state.GENPC.value))
    end
end)
handles[2] = data:install_write_tap(0xaa56, 0xaa56, 'mu4_playback_processing_latch', function(_, value, mask)
    local time = machine.time:as_double()
    if time < 0.8 or time > 2.4 then return end
    latch_writes = latch_writes + 1
    if latch_writes <= 16 then
        machine:logerror(string.format(
            'mu4_playback_latch_write: address=aa56 value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
            value, mask, latch_writes, time, cpu.state.GENPC.value))
    end
end)
_G.mu4_playback_control_taps = handles
local times, next_sample = {0.9, 1.1, 1.4}, 1
local function words(pointer)
    if pointer < 0x80 or pointer > 0xffe0 then return 'unobserved' end
    local values = {}
    for offset = 0, 31 do values[#values + 1] = string.format('%04x', data:read_u16(pointer + offset)) end
    return table.concat(values, ',')
end
emu.register_frame_done(function()
    local time = machine.time:as_double()
    if next_sample > #times or time < times[next_sample] then return end
    sampling = true
    local receiver = data:read_u16(0xaa59)
    machine:logerror(string.format(
        'mu4_playback_descriptor_snapshot: ordinal=%u address=804a words=%s t=%.9f context_only=1 observer_writes=0 audio_acceptance=0\n',
        next_sample, words(0x804a), time))
    machine:logerror(string.format(
        'mu4_playback_control_snapshot: ordinal=%u receiver=%04x words=%s aa54=%04x aa55=%04x aa56=%04x aa5a=%04x aa5b=%04x aa5c=%04x aa5d=%04x t=%.9f context_only=1 observer_writes=0 audio_acceptance=0\n',
        next_sample, receiver, words(receiver), data:read_u16(0xaa54), data:read_u16(0xaa55),
        data:read_u16(0xaa56), data:read_u16(0xaa5a), data:read_u16(0xaa5b),
        data:read_u16(0xaa5c), data:read_u16(0xaa5d), time))
    next_sample = next_sample + 1
    sampling = false
end)
