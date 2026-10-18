-- Passive address-space audit: retain taps, perform no extra reads or writes.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local taps, counts, emitted = {}, {}, {}
local boot_echo_seen = false
for _, kind in ipairs({'data', 'io'}) do
    local space = assert(cpu.spaces[kind])
    for _, address in ipairs({0x0d, 0x0e, 0x0f, 0x20, 0x21, 0x22, 0x23,
                             0x27, 0x28, 0x29, 0x30, 0x31, 0x32}) do
        for _, direction in ipairs({'read', 'write'}) do
            local key = kind .. '_' .. direction .. '_' .. address
            counts[key] = 0
            local install = direction == 'read' and space.install_read_tap or space.install_write_tap
            taps[#taps + 1] = install(space, address, address, key, function(_, value, mask)
                counts[key] = counts[key] + 1
                -- Clocked boot may poll zero beyond the generic trace cap.
                -- Preserve its first successful consumption without logging
                -- every poll or performing an extra BDRR read.
                if not boot_echo_seen and kind == 'data' and direction == 'read' and
                    address == 0x20 and value == 0x0aaa then
                    boot_echo_seen = true
                    machine:logerror(string.format('rom4_codec_boot_echo: value=0aaa pc=%04x t=%.9f\n', cpu.state['PC'].value, machine.time:as_double()))
                end
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
-- Firmware-owned speech mode and PCM ring pointers; observation only.
for _, address in ipairs({0xac, 0x0284, 0x0285, 0x0e13, 0x216d, 0x216e, 0x216f, 0x2170,
                         0x6bb, 0x6be, 0x6de, 0x6df, 0x167c, 0x167d, 0x1835, 0x1836, 0x1869,
                         0x1914, 0x1915, 0x1916, 0x1917, 0x1920, 0x1921, 0x1922, 0x1923,
                         0x195c, 0x195d, 0x197b, 0x197d}) do
    local key = 'speech_' .. address
    counts[key] = 0
    taps[#taps + 1] = cpu.spaces.data:install_write_tap(address, address, key, function(_, value, mask)
        counts[key] = counts[key] + 1
        if counts[key] <= 32 then
            machine:logerror(string.format(
                'rom4_speech_state: address=%04x value=%04x mask=%04x pc=%04x t=%.9f\n',
                address, value, mask, cpu.state['PC'].value, machine.time:as_double()))
        end
    end)
end
_G.rom4_codec_observe = {taps, counts}
