-- Passive transfer-control evidence; no completion, clocks or state writes.
-- Retire/reduce after a controller gate owns the port-4 completion contract.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local taps = {}
local controls, requests, service_requests = 0, 0, 0
local values = {}
local counter, counter_time = 0xffff, 0
taps[#taps + 1] = cpu.spaces.io:install_read_tap(0x0d, 0x0d, 'native_transfer_counter', function(_, value)
    counter, counter_time = value, machine.time:as_double()
end)
taps[#taps + 1] = cpu.spaces.io:install_write_tap(4, 4, 'native_transfer_control', function(_, value, mask)
    controls = controls + 1
    if controls > 64 then return end
    local data = cpu.spaces.data
    machine:logerror(string.format(
        'rom4_transfer_control: sequence=%d value=%04x mask=%04x pc=%04x flags=%04x,%04x pointers=%04x,%04x descriptor=%04x,%04x,%04x,%04x counter=%04x counter_age=%.9f t=%.9f\n',
        controls, value, mask, cpu.state['PC'].value,
        data:read_u16(0x195c), data:read_u16(0x195d),
        data:read_u16(0x197b), data:read_u16(0x197d),
        data:read_u16(0x1914), data:read_u16(0x1915),
        data:read_u16(0x1916), data:read_u16(0x1917), counter,
        machine.time:as_double() - counter_time, machine.time:as_double()))
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
    local keys = {}
    for value in pairs(values) do keys[#keys + 1] = value end
    table.sort(keys)
    for _, value in ipairs(keys) do
        machine:logerror(string.format('rom4_transfer_request_count: value=%04x count=%d\n', value, values[value]))
    end
    machine:logerror(string.format('rom4_transfer_summary: controls=%d requests=%d service_requests=%d completion_injected=0\n',
        controls, requests, service_requests))
end)
_G.rom4_transfer_observe = {taps = taps, stop = stop}
