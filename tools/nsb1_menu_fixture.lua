-- Extend accepted physical code entry with Menu then Phone book / Select.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_security_fixture.lua')
local machine = manager.machine
local menu
for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
    if field.mask == 4 then menu = field end
end
assert(menu, 'physical Menu contact missing')
local events = {{26, 1}, {26.2, 0}, {28, 1}, {28.2, 0}}
local next_event, next_frame = 1, 1
local times = {26.6, 28.6, 32}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        menu:set_value(event[2])
        machine:logerror(string.format('nsb1_menu_contact: index=%d down=%d t=%.9f physical_only=1\n',
            (next_event + 1) // 2, event[2], now))
        next_event = next_event + 1
    end
    if next_frame <= #times and now >= times[next_frame] then
        machine.screens[':screen']:snapshot(string.format('nsb1_menu_%d.png', next_frame))
        next_frame = next_frame + 1
    end
end, 'NSB-1 physical phonebook menu fixture')
