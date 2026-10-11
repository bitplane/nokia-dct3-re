-- Passive own-v6.39 catalogue observation; no additional memory reads.
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local totals = {}
local function observe(name, first, last)
    local sites = {}
    totals[name] = 0
    return cpu.spaces['program']:install_read_tap(first, last, name,
      function(offset, data, mask)
        totals[name] = totals[name] + 1
        local pc = cpu.state['R15'].value
        sites[pc] = (sites[pc] or 0) + 1
        if sites[pc] <= 4 then
            machine:logerror(string.format(
                '%s: address=%08x data=%08x mask=%08x pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x t=%.9f\n',
                name, offset, data, mask, pc, cpu.state['R14'].value,
                cpu.state['R0'].value, cpu.state['R1'].value, cpu.state['R2'].value,
                machine.time:as_double()))
        end
    end)
end
_G.noki3310_upload_catalogue_observer = {
    observe('3310_upload_catalogue', 0x2f35bc, 0x2f362b),
    observe('3310_upload_catalogue_ram', 0x1107c0, 0x11082f),
    emu.add_machine_stop_notifier(function()
        for name, count in pairs(totals) do
            machine:logerror(string.format('%s: total=%d\n', name, count))
        end
    end)}
