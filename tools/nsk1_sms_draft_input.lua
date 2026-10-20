-- Offline SMS composition using physical keys; no message or SIM record injection.
_G.nsk1_security_code_only = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local keys = {}
for name,cell in pairs({menu={1,4},one={2,2},two={3,2},three={4,2},zero={3,0x10},down={0,8}}) do
    for _,field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask == cell[2] then keys[name] = field end
    end
    assert(keys[name],'own SMS contact missing')
end
local sequence = {{32,'menu'},{34,'two'},{36,'three'},
    {44,'two'},{46,'zero'},{48,'menu'}}
for index=1,6 do sequence[#sequence+1] = {48+index*2,'down'} end
sequence[#sequence+1] = {62,'menu'}
local events = {}
for index,event in ipairs(sequence) do
    events[#events+1] = {event[1],event[2],1,index}
    events[#events+1] = {event[1]+0.2,event[2],0,index}
end
local index,frame = 1,1
local times = {33,35,37,39,45,47,49,61,63}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        keys[event[2]]:set_value(event[3])
        machine:logerror(string.format('nsk1_sms_draft_contact: index=%d key=%s down=%d t=%.9f physical_only=1\n',event[4],event[2],event[3],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot('nsk1_sms_draft_'..frame..'.png')
        frame = frame+1
    end
end,'NSK-1 physical offline SMS draft')
