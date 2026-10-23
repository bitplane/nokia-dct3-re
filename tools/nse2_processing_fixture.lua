-- Reuse the ROM4 passive processing census with the qualified physical UI path.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_post_auth_fixture.lua')
dofile(directory .. 'c54x_rom4_processing_observe.lua')
dofile(directory .. 'nse2_timer_queue_observe.lua')
dofile(directory .. 'nse2_sampling_observe.lua')
dofile(directory .. 'nse2_scan_request_observe.lua')
dofile(directory .. 'nse2_receive_activity_observe.lua')
dofile(directory .. 'nse2_word_port_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local counts = {}
local first_completion_wait = false
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
for _, address in ipairs({0x3204, 0x322a, 0x326d, 0x32b3, 0x32be, 0x32c5}) do
    local observed = 0
    _G.nse2_scan_loop_taps[#_G.nse2_scan_loop_taps + 1] =
        cpu.spaces.program:install_read_tap(address, address, 'nse2_scan_completion', function()
            if not first_completion_wait then return end
            local pc = cpu.state['PC'].value
            if pc ~= address and pc ~= address + 1 then return end
            observed = observed + 1
            if observed > 32 then return end
            machine:logerror(string.format('nse2_native_scan_completion: address=%04x n=%d mode=%04x skip=%04x accumulation=%04x ar2=%04x t=%.9f observer_writes=0\n',
                address, observed, cpu.spaces.data:read_u16(0xac),
                cpu.spaces.data:read_u16(0xaa), cpu.spaces.data:read_u16(0xaf),
                cpu.state['AR2'].value, machine.time:as_double()))
        end)
end
-- Bound the first few channel measurements; distinguish setup waits from
-- the processing completion wait without changing clocks or request data.
for _, address in ipairs({0x990, 0x99d, 0x9a6, 0x9b1, 0x9c2}) do
    counts[address] = 0
    _G.nse2_scan_loop_taps[#_G.nse2_scan_loop_taps + 1] =
        cpu.spaces.program:install_read_tap(address, address, 'nse2_scan_stage', function()
            local pc = cpu.state['PC'].value
            if pc ~= address and pc ~= address + 1 then return end
            counts[address] = counts[address] + 1
            if counts[address] == 1 and address == 0x9a6 then first_completion_wait = true end
            if counts[address] == 1 and address == 0x9b1 then first_completion_wait = false end
            if counts[address] > 8 then return end
            machine:logerror(string.format('nse2_native_scan_stage: address=%04x n=%d pc=%04x mode=%04x work=%04x t=%.9f observer_writes=0\n',
                address, counts[address], pc, cpu.spaces.data:read_u16(0xac),
                cpu.spaces.data:read_u16(0x6bc), machine.time:as_double()))
        end)
end
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
