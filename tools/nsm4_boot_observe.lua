-- Read-only first-boot observation. No completion, storage or handset-state writes.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local next_sample = 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now < next_sample or next_sample > 5 then return end
    machine:logerror(string.format(
        'nsm4_boot: second=%d pc=%08x lr=%08x catalogue0=%08x catalogue27=%08x read_callback=%08x program_callback=%08x dsp_control=%02x shared0=%04x shared2=%04x observer_writes=0\n',
        next_sample, cpu.state.R15.value, cpu.state.R14.value,
        memory:read_u32(0x13a9a8), memory:read_u32(0x13a9a8 + 27 * 4),
        memory:read_u32(0x13c45c), memory:read_u32(0x13c458),
        memory:read_u8(0x20002), memory:read_u16(0x10000), memory:read_u16(0x10002)))
    next_sample = next_sample + 1
end)
