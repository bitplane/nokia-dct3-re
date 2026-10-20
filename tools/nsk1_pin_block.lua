-- Three physical wrong PINs; the card owns the persistent retry transitions.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_boot_observe.lua')
local machine = manager.machine
local cells = {{2,4},{4,2},{3,2},{2,2},{1,4}}
local events = {}
for attempt,start in ipairs({12,20,28}) do
    for index,cell in ipairs(cells) do
        local field
        for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
            if candidate.mask == cell[2] then field = candidate end
        end
        assert(field,'own PIN block contact missing')
        local when = start+(index-1)*0.5
        events[#events+1] = {when,field,1,attempt,index}
        events[#events+1] = {when+0.2,field,0,attempt,index}
    end
end
local index,frame = 1,1
local times = {10,15,23,31,35,40}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk1_pin_contact: phase=block%d index=%d down=%d t=%.9f physical_only=1\n',event[4],event[5],event[3],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot('nsk1_pin_block_'..frame..'.png')
        frame = frame+1
    end
end,'NSK-1 physical PIN blocking')
