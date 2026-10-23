-- Reuse the ROM4 passive processing census with the qualified physical UI path.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_post_auth_fixture.lua')
dofile(directory .. 'c54x_rom4_processing_observe.lua')
dofile(directory .. 'nse2_timer_queue_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local counts = {}
local mcu = assert(machine.devices[':maincpu'])
local mcu_space = mcu.spaces.program
local mode_writes, constructor_entries = 0, 0
_G.nse2_request_mode_tap = mcu_space:install_write_tap(0x1094b4, 0x1094b7,
    'nse2_request_mode', function(_, value, mask)
        if (mask & 0xff) == 0 then return end
        mode_writes = mode_writes + 1
        if mode_writes > 32 then return end
        machine:logerror(string.format('nse2_request_mode: value=%02x pc=%08x t=%.9f observer_writes=0\n',
            value & 255, mcu.state['R15'].value, machine.time:as_double()))
    end)
_G.nse2_request_constructor_tap = mcu_space:install_read_tap(0x2160a8, 0x2160ab,
    'nse2_request_constructor', function()
        if mcu.state['R15'].value ~= 0x2160a8 then return end
        constructor_entries = constructor_entries + 1
        if constructor_entries > 32 then return end
        machine:logerror(string.format('nse2_request_constructor: mode=%02x lr=%08x t=%.9f observer_writes=0\n',
            mcu_space:read_u8(0x1094b7), mcu.state['R14'].value, machine.time:as_double()))
    end)
local request_summary = false
emu.register_frame_done(function()
    if request_summary or machine.time:as_double() < 30 then return end
    request_summary = true
    machine:logerror(string.format('nse2_request_summary: mode=%02x mode_writes=%d constructor_entries=%d t=%.9f observer_writes=0\n',
        mcu_space:read_u8(0x1094b7), mode_writes, constructor_entries, machine.time:as_double()))
end, 'NSE-2 passive measurement request lifecycle')
local program_dumped = false
emu.register_frame_done(function()
    if program_dumped or machine.time:as_double() < 7 then return end
    program_dumped = true
    local bytes = {}
    _G.rom4_observation_read = true
    for address = 0, 0xffff do
        local word = cpu.spaces.program:read_u16(address)
        bytes[#bytes + 1] = string.char((word >> 8) & 255, word & 255)
    end
    _G.rom4_observation_read = false
    local output = assert(io.open('nse2_native_program.bin', 'wb'))
    output:write(table.concat(bytes))
    output:close()
    machine:logerror('nse2_native_program_snapshot: words=65536 byte_order=big observer_writes=0\n')
end, 'NSE-2 passive live program snapshot')
_G.nse2_scan_loop_taps = {}
for _, address in ipairs({0x9c4, 0x9cb}) do
    counts[address] = 0
    _G.nse2_scan_loop_taps[#_G.nse2_scan_loop_taps + 1] =
        cpu.spaces.program:install_read_tap(address, address, 'nse2_scan_loop', function()
            local pc = cpu.state['PC'].value
            if pc ~= address and pc ~= address + 1 then return end
            counts[address] = counts[address] + 1
            if counts[address] > 32 then return end
            machine:logerror(string.format('nse2_native_scan_loop: address=%04x pc=%04x ar5=%04x ar6=%04x repeat=%04x t=%.9f observer_writes=0\n',
                address, pc, cpu.state['AR5'].value, cpu.state['AR6'].value,
                cpu.spaces.data:read_u16(0xe13), machine.time:as_double()))
        end)
end
