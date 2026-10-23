-- Reuse the ROM4 passive processing census with the qualified physical UI path.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_post_auth_fixture.lua')
dofile(directory .. 'c54x_rom4_processing_observe.lua')
dofile(directory .. 'nse2_timer_queue_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local counts = {}
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
