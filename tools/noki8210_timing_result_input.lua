-- Physical late-PIN input plus passive timing-result consumer observations.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki8210_registration_input.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local memory = cpu.spaces['program']
_G.noki8210_timing_result_taps = {}
for _, site in ipairs({
    {0x21ee8c, 'dispatch'}, {0x286e9a, 'receiver'},
    {0x286e58, 'update'}, {0x21eec4, 'returned'},
}) do
    local address, name = site[1], site[2]
    local count = 0
    local tap = memory:install_read_tap(address & ~3, (address & ~3) + 3,
        '8210_timing_result_' .. name, function()
            if cpu.state['PC'].value ~= address or count >= 32 then return end
            local message = cpu.state['R0'].value
            if name == 'dispatch' or name == 'returned' then
                message = memory:read_u32(cpu.state['R4'].value + 8)
            end
            if message < 0x100000 or message >= 0x17fff0 or
                    memory:read_u8(message + 3) ~= 0x88 then return end
            count = count + 1
            local record = memory:read_u32(0x137224)
            local carrier, control, errors = 0xffff, 0, 0
            if record >= 0x100000 and record < 0x17fff0 then
                carrier = memory:read_u16(record + 6)
                control = memory:read_u32(record + 12)
                errors = memory:read_u8(record + 11)
            end
            machine:logerror(string.format(
                '8210_timing_result: site=%s ordinal=%u message=%08x error=%02x target=%04x record=%08x carrier=%04x control=%08x errors=%02x t=%.6f observer_writes=0\n',
                name, count, message, memory:read_u8(message + 4),
                (memory:read_u8(message + 8) << 8) | memory:read_u8(message + 9),
                record, carrier, control, errors, machine.time:as_double()))
        end)
    _G.noki8210_timing_result_taps[#_G.noki8210_timing_result_taps + 1] = tap
end
