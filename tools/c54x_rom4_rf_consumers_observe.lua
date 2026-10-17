-- Passive full-run attribution. The last observed control write is not a read
-- of the peripheral and ordinary mode RAM is never used to drive behavior.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local taps, counts = {}, {}
local control, total = 0, 0
taps[#taps + 1] = cpu.spaces.io:install_write_tap(3, 3, 'rf_consumer_control', function(_, value, mask)
    control = (control & ~mask) | (value & mask)
end)
taps[#taps + 1] = cpu.spaces.io:install_read_tap(0x27, 0x27, 'rf_consumer_read', function()
    if _G.rom4_observation_read then return end
    local holdoff = cpu.spaces.data:read_u16(0x00aa) ~= 0 and 1 or 0
    local key = string.format('%02x:%04x:%04x:%04x:%d', cpu.state['XPC'].value, cpu.state['PC'].value,
        cpu.spaces.data:read_u16(0x00ac), control, holdoff)
    local row = counts[key]
    local now = machine.time:as_double()
    if not row then
        row = {count = 0, first = now, last = now}
        counts[key] = row
    end
    row.count, row.last = row.count + 1, now
    total = total + 1
end)
local stop = emu.add_machine_stop_notifier(function()
    local keys = {}
    for key in pairs(counts) do keys[#keys + 1] = key end
    table.sort(keys)
    for _, key in ipairs(keys) do
        local xpc, pc, mode, value, holdoff = key:match('(%x+):(%x+):(%x+):(%x+):(%d)')
        local row = counts[key]
        machine:logerror(string.format(
            'rom4_rf_consumer: xpc=%s pc=%s mode=%s control=%s holdoff=%s count=%d first=%.9f last=%.9f\n',
            xpc, pc, mode, value, holdoff, row.count, row.first, row.last))
    end
    machine:logerror(string.format('rom4_rf_consumer_summary: reads=%d buckets=%d physical_rate_claim=0 t=%.9f\n',
        total, #keys, machine.time:as_double()))
end)
_G.rom4_rf_consumers = {taps = taps, stop = stop}
