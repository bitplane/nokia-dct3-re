-- Passive own-v6.39 catalogue observation; no additional memory reads.
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local count = 0
local sites = {}
local tap = cpu.spaces['program']:install_read_tap(0x2f35bc, 0x2f362b,
    '3310_upload_catalogue', function(offset, data, mask)
        count = count + 1
        local pc = cpu.state['R15'].value
        sites[pc] = (sites[pc] or 0) + 1
        if sites[pc] <= 4 then
            machine:logerror(string.format(
                '3310_upload_catalogue: address=%08x data=%08x mask=%08x pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x t=%.9f\n',
                offset, data, mask, pc, cpu.state['R14'].value,
                cpu.state['R0'].value, cpu.state['R1'].value, cpu.state['R2'].value,
                machine.time:as_double()))
        end
    end)
_G.noki3310_upload_catalogue_observer = {tap,
    emu.add_machine_stop_notifier(function()
        machine:logerror(string.format('3310_upload_catalogue: total=%d\n', count))
    end)}
