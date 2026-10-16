-- Fixed-time RAM context only: no writer identity, instruction operands or audio claim.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'MU4 playback context requires mu4nand')
local data = assert(machine.devices[':cpu'].spaces.data)
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
    local receiver = data:read_u16(0xaa59)
    machine:logerror(string.format(
        'mu4_playback_control_snapshot: ordinal=%u receiver=%04x words=%s aa54=%04x aa55=%04x aa56=%04x aa5a=%04x aa5b=%04x aa5c=%04x aa5d=%04x t=%.9f context_only=1 observer_writes=0 audio_acceptance=0\n',
        next_sample, receiver, words(receiver), data:read_u16(0xaa54), data:read_u16(0xaa55),
        data:read_u16(0xaa56), data:read_u16(0xaa5a), data:read_u16(0xaa5b),
        data:read_u16(0xaa5c), data:read_u16(0xaa5d), time))
    next_sample = next_sample + 1
end)
