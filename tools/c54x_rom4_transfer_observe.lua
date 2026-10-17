-- Passive transfer-control evidence; no completion, clocks or state writes.
-- Retire/reduce after a controller gate owns the port-4 completion contract.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local taps = {}
local controls, requests, service_requests = 0, 0, 0
local pairs_written, pair_low = 0, 0
local pair_counts = {}
local values = {}
local counter, counter_time = 0xffff, 0
taps[#taps + 1] = cpu.spaces.io:install_read_tap(0x0d, 0x0d, 'native_transfer_counter', function(_, value)
    counter, counter_time = value, machine.time:as_double()
end)
taps[#taps + 1] = cpu.spaces.io:install_write_tap(4, 4, 'native_transfer_control', function(_, value, mask)
    controls = controls + 1
    if controls > 64 then return end
    local data = cpu.spaces.data
    if cpu.state['PC'].value == 0xa244 then
        local descriptors, payload = {}, {}
        for i = 0, 11 do
            descriptors[#descriptors + 1] = string.format('%04x', data:read_u16(0x1914 + i))
            payload[#payload + 1] = string.format('%04x', data:read_u16(0x1920 + i))
        end
        machine:logerror(string.format('rom4_transfer_table: descriptors=%s payload=%s t=%.9f\n',
            table.concat(descriptors, ','), table.concat(payload, ','), machine.time:as_double()))
    end
    machine:logerror(string.format(
        'rom4_transfer_control: sequence=%d value=%04x mask=%04x pc=%04x flags=%04x,%04x pointers=%04x,%04x descriptor=%04x,%04x,%04x,%04x counter=%04x counter_age=%.9f t=%.9f\n',
        controls, value, mask, cpu.state['PC'].value,
        data:read_u16(0x195c), data:read_u16(0x195d),
        data:read_u16(0x197b), data:read_u16(0x197d),
        data:read_u16(0x1914), data:read_u16(0x1915),
        data:read_u16(0x1916), data:read_u16(0x1917), counter,
        machine.time:as_double() - counter_time, machine.time:as_double()))
end)
taps[#taps + 1] = cpu.spaces.io:install_write_tap(0x31, 0x31, 'native_transfer_low', function(_, value)
    pair_low = value
end)
taps[#taps + 1] = cpu.spaces.io:install_write_tap(0x32, 0x32, 'native_transfer_pair', function(_, value)
    pairs_written = pairs_written + 1
    local key = string.format('%04x:%04x:%04x', cpu.state['PC'].value, pair_low, value)
    pair_counts[key] = (pair_counts[key] or 0) + 1
    if pairs_written <= 64 then
        machine:logerror(string.format('rom4_transfer_pair: sequence=%d low=%04x high=%04x pc=%04x t=%.9f\n',
            pairs_written, pair_low, value, cpu.state['PC'].value, machine.time:as_double()))
    end
end)
taps[#taps + 1] = cpu.spaces.io:install_read_tap(1, 1, 'native_transfer_request', function(_, value, mask)
    requests = requests + 1
    values[value] = (values[value] or 0) + 1
    if (value & 0x1000) ~= 0 then
        service_requests = service_requests + 1
        if service_requests <= 32 then
            machine:logerror(string.format('rom4_transfer_request: value=%04x mask=%04x pc=%04x t=%.9f\n',
                value, mask, cpu.state['PC'].value, machine.time:as_double()))
        end
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    local pair_keys = {}
    for key in pairs(pair_counts) do pair_keys[#pair_keys + 1] = key end
    table.sort(pair_keys)
    for _, key in ipairs(pair_keys) do
        local pc, low, high = key:match('(%x+):(%x+):(%x+)')
        machine:logerror(string.format('rom4_transfer_pair_count: pc=%s low=%s high=%s count=%d\n',
            pc, low, high, pair_counts[key]))
    end
    local keys = {}
    for value in pairs(values) do keys[#keys + 1] = value end
    table.sort(keys)
    for _, value in ipairs(keys) do
        machine:logerror(string.format('rom4_transfer_request_count: value=%04x count=%d\n', value, values[value]))
    end
    machine:logerror(string.format('rom4_transfer_summary: controls=%d requests=%d service_requests=%d pairs=%d completion_injected=0 t=%.9f\n',
        controls, requests, service_requests, pairs_written, machine.time:as_double()))
end)
_G.rom4_transfer_observe = {taps = taps, stop = stop}
