-- Reuse the ROM4 passive processing census with the qualified physical UI path.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_post_auth_fixture.lua')
dofile(directory .. 'c54x_rom4_processing_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local counts = {}
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
