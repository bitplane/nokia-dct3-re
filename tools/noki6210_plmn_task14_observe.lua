-- Bounded own-ROM task-14 publication census; observations only.
local machine = manager.machine
assert(machine.system.name == 'npe3hle', 'requires NPE-3 research-HLE composition')
local cpu = assert(machine.devices[':maincpu'])
local space = cpu.spaces['program']
local busy, posts = false, 0
local tap = space:install_read_tap(0x3c22c0, 0x3c22c3,
    'plmn_task14_census', function(address)
        if busy or address ~= 0x3c22c0 or cpu.state['PC'].value ~= 0x3c22c0
            or cpu.state['R0'].value ~= 14 or posts >= 160 then return end
        local object = cpu.state['R1'].value
        if object < 0x100000 or object + 9 >= 0x180000 then return end
        busy = true
        local status = space:read_u16(object)
        busy = false
        posts = posts + 1
        machine:logerror(string.format(
            'plmn_task14_census: status=%04x caller=%08x t=%.9f\n',
            status, cpu.state['R14'].value, machine.time:as_double()))
    end)
_G.plmn_task14_census_observer = {tap = tap}
