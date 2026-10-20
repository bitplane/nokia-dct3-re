-- Physical erase navigation; persisted card contents are changed only by firmware.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local sequence = {{30,'down'},{31,'down'},{33,'menu'},{35,'menu'},{37,'menu'},{39,'menu'}}
local keys = {}
for name,cell in pairs({down={0,8},menu={1,4}}) do
    for _,field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask == cell[2] then keys[name] = field end
    end
    assert(keys[name],'own erase contact missing')
end
local events = {}
for index,event in ipairs(sequence) do
    events[#events+1] = {event[1],event[2],1,index}
    events[#events+1] = {event[1]+0.2,event[2],0,index}
end
local next_event,next_frame = 1,1
local frames = {32,34,36,38,40,44,50}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        keys[event[2]]:set_value(event[3])
        machine:logerror(string.format('nsk1_phonebook_erase: index=%d key=%s down=%d t=%.9f physical_only=1\n',event[4],event[2],event[3],now))
        next_event = next_event+1
    end
    if next_frame <= #frames and now >= frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk1_phonebook_erase_'..next_frame..'.png')
        next_frame = next_frame+1
    end
end,'NSK-1 physical contact deletion')
