-- Passive candidate census, not a decoded Nokia DSP-stage waveform.
-- TI SPRU131G table 4-2 identifies ST1 bit 13 as architectural XF.
local machine = manager.machine
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local program = dsp.spaces['program']
local data = dsp.spaces['data']
local candidates = {0x2603, 0x2604, 0x260b, 0x260c, 0x2613, 0x2615, 0x309c}
local addresses, taps, counts = {}, {}, {}
local observing = false
for _, pc in ipairs(candidates) do
    addresses[pc], addresses[(pc + 1) & 0xffff] = true, true
end
addresses[0x2523] = true
for pc in pairs(addresses) do
    local address = pc
    counts[address] = 0
    taps[#taps + 1] = program:install_read_tap(address, address, 'rom4_xf_' .. address,
        function(offset, word)
            if observing then return end
            if dsp.state['PC'].value ~= address + 1 then return end
            counts[address] = counts[address] + 1
            if counts[address] > 16 then return end
            observing = true
            local st1 = dsp.state['ST1'].value
            machine:logerror(string.format(
                'rom4_xf_fetch: pc=%04x word=%04x hit=%d st1=%04x xf=%d t=%.9f\n',
                address, word, counts[address], st1, (st1 >> 13) & 1,
                machine.time:as_double()))
            if address == 0x2523 or address == 0x2603 or address == 0x260b or address == 0x2613 or address == 0x2615 then
                local sp, stack = dsp.state['SP'].value, {}
                for index = 0, 5 do
                    stack[#stack + 1] = string.format('%04x', data:read_u16((sp + index) & 0xffff))
                end
                machine:logerror(string.format('rom4_xf_context: pc=%04x hit=%d sp=%04x stack=%s\n',
                    address, counts[address], sp, table.concat(stack, ',')))
                if address == 0x2523 then
                    local caller, dispatch = {}, {}
                    local top = data:read_u16(sp)
                    for index = 0, 15 do
                        caller[#caller + 1] = string.format('%04x', program:read_u16((top - 8 + index) & 0xffff))
                        dispatch[#dispatch + 1] = string.format('%04x', program:read_u16(0x2523 + index))
                    end
                    machine:logerror(string.format(
                        'rom4_xf_dispatch: hit=%d b=%010x stack_top=%04x caller=%s dispatch=%s\n',
                        counts[address], dsp.state['B'].value, top,
                        table.concat(caller, ','), table.concat(dispatch, ',')))
                end
            end
            observing = false
        end)
end
_G.rom4_xf_observer = {taps = taps, stop = emu.add_machine_stop_notifier(function()
    for _, pc in ipairs(candidates) do
        machine:logerror(string.format('rom4_xf_candidate: pc=%04x count=%d\n', pc, counts[pc]))
    end
end)}
