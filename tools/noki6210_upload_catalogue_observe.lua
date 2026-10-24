-- Passive own-ROM catalogue reads; no extra memory reads or DSP requests.
local source = debug.getinfo(1, 'S').source:sub(2)
if not _G.npe3_catalogue_parent_observer then
    dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
end
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local totals = {}
local function observe(name, first, last)
    local sites = {}
    totals[name] = 0
    return cpu.spaces.program:install_read_tap(first, last, name,
        function(address, data, mask)
            totals[name] = totals[name] + 1
            local pc = cpu.state.R15.value
            sites[pc] = (sites[pc] or 0) + 1
            if sites[pc] <= 32 then
                machine:logerror(string.format(
                    '%s: address=%08x data=%08x mask=%08x pc=%08x r0=%08x r1=%08x r2=%08x t=%.9f\n',
                    name, address, data, mask, pc, cpu.state.R0.value,
                    cpu.state.R1.value, cpu.state.R2.value, machine.time:as_double()))
            end
        end)
end
_G.npe3_catalogue_observers = {
    observe('npe3_catalogue_flash', 0x515994, 0x515a03),
    observe('npe3_catalogue_ram', 0x170004, 0x170073),
    emu.add_machine_stop_notifier(function()
        for name, count in pairs(totals) do
            machine:logerror(string.format('%s: total=%d\n', name, count))
        end
    end)}
