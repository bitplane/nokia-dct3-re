-- Passive native ROM4 serial-control comparison; no I/O or firmware writes.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local handles, counts, serial = {}, {}, {}
for _, direction in ipairs({'read', 'write'}) do
  for _, port in ipairs({0x00, 0x02, 0x34, 0x35, 0x36}) do
    local sites = {}
    local install = direction == 'read' and 'install_read_tap' or 'install_write_tap'
    handles[#handles + 1] = cpu.spaces['io'][install](cpu.spaces['io'], port, port,
        'rom4_serial_' .. port .. '_' .. direction, function(offset, value, mask)
            local pc = cpu.state['PC'].value
            local key = port .. '_' .. direction
            local state = serial[key] or {count = 0}
            state.count, state.value, state.pc = state.count + 1, value, pc
            serial[key] = state
            sites[pc] = (sites[pc] or 0) + 1
            if sites[pc] > 4 then return end
            if port == 0x36 then
                machine:logerror(string.format(
                    'rom4_port36: direction=%s value=%04x pc=%04x t=%.9f\n',
                    direction, value, pc, machine.time:as_double()))
            else
                machine:logerror(string.format(
                    'rom4_serial_port: port=%02x direction=%s value=%04x pc=%04x t=%.9f\n',
                    port, direction, value, pc, machine.time:as_double()))
            end
        end)
  end
end
for _, site in ipairs({0x3bcd, 0x9232, 0x9129, 0x9140}) do
    counts[site] = 0
    handles[#handles + 1] = cpu.spaces['program']:install_read_tap(site, site,
        'rom4_port36_site_' .. site, function()
            if cpu.state['PC'].value == site + 1 then
                counts[site] = counts[site] + 1
            end
        end)
end
local stop = emu.add_machine_stop_notifier(function()
    for _, port in ipairs({0x00, 0x02, 0x34, 0x35, 0x36}) do
        for _, direction in ipairs({'read', 'write'}) do
            local state = serial[port .. '_' .. direction]
            machine:logerror(string.format(
                'rom4_serial_summary: port=%02x direction=%s count=%d last_value=%04x last_pc=%04x\n',
                port, direction, state and state.count or 0,
                state and state.value or 0, state and state.pc or 0))
        end
    end
    for site, count in pairs(counts) do
        machine:logerror(string.format('rom4_port36_count: site=%04x count=%d\n', site, count))
    end
end)
_G.rom4_port36_handles = {handles = handles, stop = stop}
