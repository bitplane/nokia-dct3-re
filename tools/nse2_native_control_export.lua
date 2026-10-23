-- Standalone read-only export: use -debug -debugger none -autoboot_delay 0.
-- Wait for the firmware upload: the low program overlay is zero at cold entry.
local machine = manager.machine
assert(machine.system.name == 'nse2r4v' or machine.system.name == 'nse2r4t')
assert(machine.devices[':dsp_c54x:cpu'], 'native DSP required')
local done = false
emu.register_frame_done(function()
    if done or machine.time:as_double() < 7 then return end
    done = true
    local debugger = assert(machine.debugger, 'enable MAME debugger for control export')
    local program, bytes = machine.devices[':dsp_c54x:cpu'].spaces.program, {}
    for address = 0, 0xffff do
        local word = program:read_u16(address)
        bytes[#bytes + 1] = string.char((word >> 8) & 255, word & 255)
    end
    local output = assert(io.open('nse2_native_program.bin', 'wb'))
    output:write(table.concat(bytes))
    output:close()
    for _, range in ipairs({{'modes',0x4414,0xc0}, {'request',0x4dce,0x100},
                            {'measurement',0x926,0x15a}, {'filter',0x3380,0x90},
                            {'frame',0x3204,0x100}, {'wait',0x407c,0x70},
                            {'rssi_publisher',0x47f6,0x40}, {'rssi_host',0x3c80,0x60},
                            {'rssi_other',0x6d40,0x70}, {'host_dispatch',0x3970,0x30},
                            {'eligibility',0x6760,0x90}}) do
        debugger:command(string.format('dasm nse2_control_%s.dasm,%x,%x,1,:dsp_c54x:cpu',
            range[1], range[2], range[3]))
    end
    machine:exit()
end, 'NSE-2 native control export after upload')
