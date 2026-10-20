-- Passive own-image MCU/storage/DSP boundary observations; no guest writes.
local machine = manager.machine
assert(machine.system.name == 'noki5190')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local shared_writes = 0
_G.nsb1_shared_tap = space:install_write_tap(0x10000, 0x10fff, 'nsb1_shared', function(offset, data, mask)
    shared_writes = shared_writes + 1
    if shared_writes <= 12 then
        machine:logerror(string.format('nsb1_shared: address=%08x data=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',
            offset, data, mask, cpu.state['R15'].value, machine.time:as_double()))
    end
end)
local times, next_sample = {0.1, 0.5, 1, 2, 5}, 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if next_sample > #times or now < times[next_sample] then return end
    if next_sample == 3 then
        local program = {}
        for index = 0, 185 do
            program[#program + 1] = string.format('%04x', space:read_u16(0x11e00 + index * 2))
        end
        machine:logerror('nsb1_program: words=186 data=' .. table.concat(program) .. ' observer_writes=0\n')
    end
    machine:logerror(string.format('nsb1_boot: t=%.9f pc=%08x sp=%08x ctrl=%02x eeprom_mode=%02x page=%02x capacity=%08x selector=%02x dsp_family=%04x catalogue0=%08x/%08x shared_writes=%d observer_writes=0\n',
        now, cpu.state['R15'].value, cpu.state['R13'].value, space:read_u8(0x2000b),
        space:read_u8(0x10c4a4), space:read_u8(0x10c4a5), space:read_u32(0x10c4a8),
        space:read_u8(0x10c466), space:read_u16(0x10ad2e),
        space:read_u32(0x10adc0), space:read_u32(0x10ad50), shared_writes))
    next_sample = next_sample + 1
end, 'NSB-1 passive MCU boundary')
