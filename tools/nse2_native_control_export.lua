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
    local data_bytes = {}
    local data = machine.devices[':dsp_c54x:cpu'].spaces.data
    for address = 0, 0xffff do
        -- Do not sample mapped CPU/serial registers: reads may acknowledge
        -- peripheral state. Preserve file offsets with an explicit zero pad.
        local word = address < 0x80 and 0 or data:read_u16(address)
        data_bytes[#data_bytes + 1] = string.char((word >> 8) & 255, word & 255)
    end
    local data_output = assert(io.open('nse2_native_data.bin', 'wb'))
    data_output:write(table.concat(data_bytes))
    data_output:close()
    for _, range in ipairs({{'modes',0x4414,0xc0}, {'request',0x4dce,0x100},
                            {'measurement',0x926,0x15a}, {'filter',0x3380,0x90},
                            {'frame',0x3204,0x100}, {'wait',0x407c,0x70},
                            {'rssi_publisher',0x47f6,0x40}, {'rssi_host',0x3c80,0x60},
                            {'rssi_other',0x6d40,0x70}, {'host_dispatch',0x3970,0x30},
                            {'eligibility',0x6760,0x90}, {'result_requests',0x3bf0,0x180},
                            {'shared_notification',0x382f,0x70},
                            {'peripheral_dispatch',0x3598,0x240},
                            {'sampling_stop',0x44b6,0xc0},
                            {'peripheral_alternate',0x8e89,0x80},
                            {'peripheral_alternate_b',0xb787,0xa0},
                            {'sampling_reader',0x71d0,0x70},
                            {'scan_request',0x3d40,0xb0},
                            {'peripheral_tx',0x9162,0x100},
                            {'word_init',0x8d00,0x180},
                            {'word_control',0x90a0,0xc2},
                            {'word_setup',0x9009,0x97},
                            {'word_host_config',0x3e20,0x70},
                            {'word_all_control',0x8d00,0x600},
                            {'word_all_alternate',0xb780,0x180},
                            {'word_serial_isr',0x3416,0x140},
                            {'word_config_host',0x3b80,0x70},
                            {'word_service_loop',0x3924,0x4c},
                            {'word_service_alternate',0x3f4e,0x42},
                            {'word_frame_builder',0x92df,0x6d},
                            {'word_host_forward',0x3ee7,0x22},
                            {'vectors',0xff80,0x80}}) do
        debugger:command(string.format('dasm nse2_control_%s.dasm,%x,%x,1,:dsp_c54x:cpu',
            range[1], range[2], range[3]))
    end
    -- These are the reviewed page-zero raw direct CALLD candidates, not
    -- asserted reachable producers. Export their argument/gate context.
    for _, address in ipairs({0x314d, 0x3fe2, 0x3ff5, 0x4810, 0x4a11,
                              0x4a21, 0x4a37, 0x4a68, 0x4ab0, 0x4b1a, 0x4ba8,
                              0x4be9, 0x4f20, 0x50db, 0x6073, 0x62fe, 0x6ec6,
                              0x8e1c, 0x9404}) do
        debugger:command(string.format('dasm nse2_publisher_%04x.dasm,%x,40,1,:dsp_c54x:cpu',
            address, address - 0x18))
    end
    machine:exit()
end, 'NSE-2 native control export after upload')
