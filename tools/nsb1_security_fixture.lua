-- Physical default-code entry and read-only observation of the own retry gate.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_boot_observe.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local space = cpu.spaces['program']
_G.nsb1_security_read = space:install_read_tap(0x10c498, 0x10c49b, 'nsb1_security_retry', function(offset, data, mask)
    if cpu.state['R15'].value == 0x2ab978 and mask == 0xff000000 then
        machine:logerror(string.format('nsb1_security_gate: retry=%02x match=%08x t=%.9f observer_writes=0\n',
            (data >> 24) & 0xff, cpu.state['R0'].value, machine.time:as_double()))
    end
end)
local events = {}
for index, contact in ipairs({{2, 2}, {3, 2}, {4, 2}, {2, 4}, {3, 4}, {1, 4}}) do
    local field
    for _, candidate in pairs(machine.ioport.ports[':COL.' .. contact[1]].fields) do
        if candidate.mask == contact[2] then field = candidate end
    end
    assert(field, 'own keypad contact missing')
    events[#events + 1] = {12 + (index - 1) * 0.4, field, 1, index}
    events[#events + 1] = {12.2 + (index - 1) * 0.4, field, 0, index}
end
local next_event, next_frame = 1, 1
local times = {13.9, 14.5, 18, 25}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsb1_phonecode_contact: index=%d down=%d t=%.9f physical_only=1\n',
            event[4], event[3], now))
        next_event = next_event + 1
    end
    if next_frame <= #times and now >= times[next_frame] then
        machine.screens[':screen']:snapshot(string.format('nsb1_phonecode_%d.png', next_frame))
        next_frame = next_frame + 1
    end
end, 'NSB-1 physical security-code fixture')
