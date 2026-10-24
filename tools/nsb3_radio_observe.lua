-- Own 29419e/29421a transmit grammar; no synthesized DSP/network replies.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_phonecode_input.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local cursor, count, invalid = 0, 0, 0
local rx_cursor, rx_count, rx_invalid = 0x80, 0, 0
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
cpu.debug:bpset(0x28a5a2, '1',
    'logerror "nsb3_radio_8b_forward: message=%08x header=%08x observer_writes=0\\n",r0,d@r0;g')
_G.nsb3_radio_rx = dsp.spaces.data:install_write_tap(0x08e4, 0x08e4, 'nsb3_radio_rx', function(_, producer)
    if producer < 0x80 or producer >= 0xe4 then return end
    while rx_cursor ~= producer and rx_count < 128 do
        local header = memory:read_u16(0x10000 + rx_cursor * 2)
        local length = header >> 8
        local words = (length + 3) // 2
        if words > (producer - rx_cursor) % 0x64 then
            rx_invalid = rx_invalid + 1
            rx_cursor = producer
            return
        end
        local body = {}
        for index = 0, length - 1 do
            local position = 0x80 + ((rx_cursor - 0x80 + 1 + index // 2) % 0x64)
            local packed = memory:read_u16(0x10000 + position * 2)
            body[#body + 1] = string.format('%02x', (index % 2 == 0 and packed >> 8 or packed) & 255)
        end
        rx_count = rx_count + 1
        machine:logerror(string.format('nsb3_radio_rx: index=%d type=%02x length=%d data=%s cursor=%02x producer=%02x dsp_pc=%04x t=%.9f observer_writes=0\n',
            rx_count, header & 255, length, table.concat(body), rx_cursor, producer, dsp.state['PC'].value, machine.time:as_double()))
        rx_cursor = 0x80 + ((rx_cursor - 0x80 + words) % 0x64)
    end
end)
_G.nsb3_radio_tx = memory:install_write_tap(0x100a4, 0x100a7, 'nsb3_radio_tx', function(_, value, mask)
    if mask & 0xffff0000 == 0 then return end
    local producer = (value >> 16) & 0xffff
    if producer >= 0x52 then
        invalid = invalid + 1
        return
    end
    while cursor ~= producer and count < 256 do
        local header = memory:read_u16(0x10000 + cursor * 2)
        local length = header >> 8
        local words = (length + 3) // 2
        if words > (producer - cursor) % 0x52 then
            invalid = invalid + 1
            machine:logerror(string.format('nsb3_radio_invalid: cursor=%02x producer=%02x header=%04x observer_writes=0\n', cursor, producer, header))
            cursor = producer
            return
        end
        local body = {}
        for index = 0, length - 1 do
            local packed = memory:read_u16(0x10000 + ((cursor + 1 + index // 2) % 0x52) * 2)
            body[#body + 1] = string.format('%02x', (index % 2 == 0 and packed >> 8 or packed) & 255)
        end
        count = count + 1
        local secret = header & 255 == 0x14 and length >= 10 and body[1] ~= '00'
        machine:logerror(string.format('nsb3_radio_tx: index=%d type=%02x length=%d data=%s cursor=%02x producer=%02x pc=%08x t=%.9f observer_writes=0\n',
            count, header & 255, length, secret and '<redacted>' or table.concat(body), cursor, producer,
            cpu.state['PC'].value, machine.time:as_double()))
        cursor = (cursor + words) % 0x52
    end
end)
local samples = coroutine.create(function()
    for _, time in ipairs({8, 20, 35, 45, 60}) do
        assert(emu.wait(time - machine.time:as_double()))
        machine:logerror(string.format('nsb3_radio_state: tx=%d invalid=%d producer=%04x consumer=%04x rx_producer=%04x rx_consumer=%04x dsp_pc=%04x illegal=%d t=%.9f observer_writes=0\n',
            count, invalid, memory:read_u16(0x100a4), memory:read_u16(0x100a6),
            memory:read_u16(0x101c8), memory:read_u16(0x101ca),
            dsp.state['PC'].value, dsp.state['ILLEGAL'].value, machine.time:as_double()))
        machine:logerror(string.format('nsb3_radio_receive_count: packets=%d invalid=%d t=%.9f observer_writes=0\n', rx_count, rx_invalid, machine.time:as_double()))
    end
end)
assert(coroutine.resume(samples))
