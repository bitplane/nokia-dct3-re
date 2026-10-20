-- Physical Add entry workflow; contact contents are never written by Lua.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_menu_fixture.lua')
local machine = manager.machine
local keys = {
    down = {0, 8}, menu = {1, 4},
    one = {2, 2}, two = {3, 2}, three = {4, 2},
}
local sequence = {
    {30, 'down'}, {32, 'menu'},
    {34, 'two'}, {34.6, 'three'}, {35.2, 'two'}, {36.5, 'menu'},
    {38, 'one'}, {38.6, 'two'}, {39.2, 'three'}, {40.5, 'menu'},
}
local events = {}
for index, event in ipairs(sequence) do
    local contact, field = keys[event[2]], nil
    for _, candidate in pairs(machine.ioport.ports[':COL.' .. contact[1]].fields) do
        if candidate.mask == contact[2] then field = candidate end
    end
    assert(field, 'physical phonebook contact missing')
    events[#events + 1] = {event[1], field, 1, index, event[2]}
    events[#events + 1] = {event[1] + 0.2, field, 0, index, event[2]}
end
local times = {33, 36, 37, 40, 41.5, 44}
local next_event, next_frame = 1, 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsb1_phonebook_contact: index=%d key=%s down=%d t=%.9f physical_only=1\n',
            event[4], event[5], event[3], now))
        next_event = next_event + 1
    end
    if next_frame <= #times and now >= times[next_frame] then
        machine.screens[':screen']:snapshot(string.format('nsb1_phonebook_%d.png', next_frame))
        next_frame = next_frame + 1
    end
end, 'NSB-1 physical SIM phonebook fixture')
