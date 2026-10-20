-- Cold-boot search of the contact persisted by nsb1_phonebook_fixture.lua.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_menu_fixture.lua')
local machine = manager.machine
local menu
for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
    if field.mask == 4 then menu = field end
end
assert(menu, 'physical search contact missing')
local events = {{30, 1}, {30.2, 0}, {32, 1}, {32.2, 0}}
local next_event, captured = 1, false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        menu:set_value(event[2])
        machine:logerror(string.format('nsb1_phonebook_search: index=%d down=%d t=%.9f physical_only=1\n',
            (next_event + 1) // 2, event[2], now))
        next_event = next_event + 1
    end
    if now >= 33 and not captured then
        machine.screens[':screen']:snapshot('nsb1_phonebook_reloaded.png')
        captured = true
    end
end, 'NSB-1 physical cold phonebook search')
