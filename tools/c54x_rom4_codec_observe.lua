-- Passive address-space audit: retain taps, perform no extra reads or writes.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local taps, counts, emitted = {}, {}, {}
for _, kind in ipairs({'data', 'io'}) do
    local space = assert(cpu.spaces[kind])
    for _, address in ipairs({0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32}) do
        for _, direction in ipairs({'read', 'write'}) do
            local key = kind .. '_' .. direction .. '_' .. address
            counts[key] = 0
            local install = direction == 'read' and space.install_read_tap or space.install_write_tap
            taps[#taps + 1] = install(space, address, address, key, function(_, value, mask)
                counts[key] = counts[key] + 1
                local phase = machine.time:as_double() >= 7 and 'interactive' or 'boot'
                local output_key = key .. '_' .. phase
                emitted[output_key] = (emitted[output_key] or 0) + 1
                if emitted[output_key] <= 16 then
                    machine:logerror(string.format(
                        'rom4_serial_audit: space=%s direction=%s address=%04x value=%04x mask=%04x pc=%04x t=%.9f\n',
                        kind, direction, address, value, mask, cpu.state['PC'].value, machine.time:as_double()))
                end
            end)
        end
    end
end
_G.rom4_codec_observe = {taps, counts}
