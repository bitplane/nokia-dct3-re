-- Fresh-process lookup of the card record written by the phone.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local events = {}
for index,event in ipairs({{30,1,4,'menu'},{32,3,2,'two'},{34,1,4,'menu'}}) do
    local selected
    for _,field in pairs(machine.ioport.ports[':COL.'..event[2]].fields) do
        if field.mask == event[3] then selected = field end
    end
    assert(selected,'own physical Search contact missing')
    events[#events+1] = {event[1],selected,1,index,event[4]}
    events[#events+1] = {event[1]+0.2,selected,0,index,event[4]}
end
local next_event,next_frame = 1,1
local frames = {35,38,40}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk1_phonebook_search: index=%d key=%s down=%d t=%.9f physical_only=1\n',event[4],event[5],event[3],now))
        next_event = next_event+1
    end
    if next_frame <= #frames and now >= frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk1_phonebook_reloaded_'..next_frame..'.png')
        next_frame = next_frame+1
    end
end,'NSK-1 physical cold contact search')
