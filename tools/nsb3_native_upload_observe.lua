-- Observe upload extent before admitting a wider program overlay.
local machine = manager.machine
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local writes, highest = 0, 0
_G.nsb3_upload_tap = dsp.spaces.program:install_write_tap(0x2800, 0x2fff,
    'nsb3_program_extent', function(address, data, mask)
        writes = writes + 1
        highest = math.max(highest, address)
        if writes <= 8 then
            machine:logerror(string.format('nsb3_program_write: address=%04x data=%04x mask=%04x observer_writes=0\n', address, data, mask))
        end
    end)
local captured = false
emu.register_frame_done(function()
    if captured or machine.time:as_double() < 1 then return end
    captured = true
    machine:logerror(string.format('nsb3_program_extent: writes=%d highest=%04x observer_writes=0\n', writes, highest))
    local file = assert(io.open('nsb3_program_2800.bin', 'wb'))
    for address = 0x2800, 0x2fff do
        file:write(string.pack('>I2', dsp.spaces.program:read_u16(address)))
    end
    file:close()
end)
