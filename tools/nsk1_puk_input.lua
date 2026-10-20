-- Physical PUK recovery; the card receives the replacement PIN over SIMI.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_boot_observe.lua')
local machine = manager.machine
local cells = {['1']={2,2},['2']={3,2},['3']={4,2},['4']={2,4},
    ['5']={3,4},['6']={4,4},['7']={2,8},['8']={3,8},ok={1,4}}
local events = {}
for _,phase in ipairs({{12,'puk',_G.nsk1_wrong_puk and '87654321' or '12345678'},{20,'new','4321'},{26,'confirm','4321'}}) do
    local names = {}
    for name in phase[3]:gmatch('.') do names[#names+1] = name end
    names[#names+1] = 'ok'
    for index,name in ipairs(names) do
        local cell,field = cells[name],nil
        for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
            if candidate.mask == cell[2] then field = candidate end
        end
        assert(field,'own PUK contact missing')
        local when = phase[1]+(index-1)*0.5
        events[#events+1] = {when,field,1,phase[2],index}
        events[#events+1] = {when+0.2,field,0,phase[2],index}
    end
end
local index,frame = 1,1
local times = {10,18,24,30,35,40}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk1_pin_contact: phase=%s index=%d down=%d t=%.9f physical_only=1\n',event[4],event[5],event[3],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot('nsk1_puk_unblock_'..frame..'.png')
        frame = frame+1
    end
end,'NSK-1 physical PUK recovery')
