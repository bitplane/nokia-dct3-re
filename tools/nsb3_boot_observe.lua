-- Read-only initial NSB-3 execution instrument; no handset-state writes.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local next_sample = 1
emu.register_frame_done(function()
    if machine.time:as_double() < next_sample or next_sample > 5 then return end
    machine:logerror(string.format(
        'nsb3_boot: second=%d pc=%08x lr=%08x selector=%04x catalogue3=%08x catalogue4=%08x dsp_control=%02x shared0=%04x shared2=%04x observer_writes=0\n',
        next_sample, cpu.state.R15.value, cpu.state.R14.value,
        memory:read_u16(0x10ad0a), memory:read_u32(0x10ad9c), memory:read_u32(0x10ad2c),
        memory:read_u8(0x20002), memory:read_u16(0x10000), memory:read_u16(0x10002)))
    next_sample = next_sample + 1
end)
