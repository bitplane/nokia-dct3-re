-- Passive candidate census, not a decoded Nokia DSP-stage waveform.
-- TI SPRU131G table 4-2 identifies ST1 bit 13 as architectural XF.
local machine = manager.machine
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local program = dsp.spaces['program']
local candidates = {0x2603, 0x2604, 0x260b, 0x260c, 0x2613, 0x2615, 0x309c}
local addresses, taps, counts = {}, {}, {}
for _, pc in ipairs(candidates) do
    addresses[pc], addresses[(pc + 1) & 0xffff] = true, true
end
for pc in pairs(addresses) do
    local address = pc
    counts[address] = 0
    taps[#taps + 1] = program:install_read_tap(address, address, 'rom4_xf_' .. address,
        function(offset, word)
            if dsp.state['PC'].value ~= address + 1 then return end
            counts[address] = counts[address] + 1
            if counts[address] > 16 then return end
            local st1 = dsp.state['ST1'].value
            machine:logerror(string.format(
                'rom4_xf_fetch: pc=%04x word=%04x hit=%d st1=%04x xf=%d t=%.9f\n',
                address, word, counts[address], st1, (st1 >> 13) & 1,
                machine.time:as_double()))
        end)
end
_G.rom4_xf_observer = {taps = taps, stop = emu.add_machine_stop_notifier(function()
    for _, pc in ipairs(candidates) do
        machine:logerror(string.format('rom4_xf_candidate: pc=%04x count=%d\n', pc, counts[pc]))
    end
end)}
