-- Wrong SIM PIN via own mapped keys, without entering the phone security code.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_boot_observe.lua')
local machine = manager.machine
local sequence = {{2,4},{4,2},{3,2},{2,2},{1,4}}
local events = {}
for index,contact in ipairs(sequence) do
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..contact[1]].fields) do
        if candidate.mask == contact[2] then field = candidate end
    end
    assert(field,'own SIM PIN contact missing')
    events[#events+1] = {12+(index-1)*0.5,field,1,index}
    events[#events+1] = {12.2+(index-1)*0.5,field,0,index}
end
local next_event,next_frame = 1,1
local times = {10,15,17,20}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk1_pin_contact: phase=wrong index=%d down=%d t=%.9f physical_only=1\n',event[4],event[3],now))
        next_event = next_event+1
    end
    if next_frame <= #times and now >= times[next_frame] then
        machine.screens[':screen']:snapshot('nsk1_pin_wrong_'..next_frame..'.png')
        next_frame = next_frame+1
    end
end,'NSK-1 physical wrong SIM PIN')
