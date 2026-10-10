-- Passive native ROM4 port-36 comparison; no I/O or firmware writes.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local handles, counts = {}, {}
for _, direction in ipairs({'read', 'write'}) do
    local sites = {}
    local install = direction == 'read' and 'install_read_tap' or 'install_write_tap'
    handles[#handles + 1] = cpu.spaces['io'][install](cpu.spaces['io'], 0x36, 0x36,
        'rom4_port36_' .. direction, function(offset, value, mask)
            local pc = cpu.state['PC'].value
            sites[pc] = (sites[pc] or 0) + 1
            if sites[pc] > 4 then return end
            machine:logerror(string.format(
                'rom4_port36: direction=%s value=%04x pc=%04x t=%.9f\n',
                direction, value, pc, machine.time:as_double()))
        end)
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
    for site, count in pairs(counts) do
        machine:logerror(string.format('rom4_port36_count: site=%04x count=%d\n', site, count))
    end
end)
_G.rom4_port36_handles = {handles = handles, stop = stop}
