-- Search an independently persisted empty card through physical contacts.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local keys = {}
for name,cell in pairs({menu={1,4},two={3,2}}) do
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask == cell[2] then keys[name] = candidate end
    end
    assert(keys[name],'own Search contact missing')
end
local events = {{30,1,1,'menu'},{30.2,0,1,'menu'},
    {32,1,2,'two'},{32.2,0,2,'two'},{34,1,3,'menu'},{34.2,0,3,'menu'}}
local index,frame = 1,1
local times = {35,38,40}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        keys[event[4]]:set_value(event[2])
        machine:logerror(string.format('nsk1_phonebook_empty: index=%d key=%s down=%d t=%.9f physical_only=1\n',event[3],event[4],event[2],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot('nsk1_phonebook_empty_'..frame..'.png')
        frame = frame+1
    end
end,'NSK-1 physical cold empty search')
