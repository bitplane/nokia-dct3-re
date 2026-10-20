-- Offline SIM draft only; every action uses physical matrix contacts.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_security_fixture.lua')
local machine = manager.machine
local keys = {}
for name, definition in pairs({menu={1,4},one={2,2},two={3,2},three={4,2},zero={3,0x10},down={0,8}}) do
    for _, field in pairs(machine.ioport.ports[':COL.' .. definition[1]].fields) do
        if field.mask == definition[2] then keys[name] = field end
    end
    assert(keys[name], 'own SMS contact missing: ' .. name)
end
local reload = _G.nsb1_sms_reload == true
local sequence, times
if reload then
    sequence = {{26,'menu'},{28,'two'},{30,'one'},{32,'two'},{36,'menu'}}
    times = {35,37,39}
    if _G.nsb1_sms_empty then
        times = {37}
    end
else
    sequence = {{26,'menu'},{28,'two'},{30,'one'},{32,'three'},{38,'two'},{40,'zero'},{42,'menu'}}
    -- Save is option 7; Dictionary precedes Send special in this own list.
    for i=1,6 do sequence[#sequence+1] = {42+2*i,'down'} end
    sequence[#sequence+1] = {56,'menu'}
    times = {39,41,43,55,57,59}
end
local events = {}
for _, event in ipairs(sequence) do
    events[#events+1] = {event[1],event[2],1}
    events[#events+1] = {event[1]+0.2,event[2],0}
end
local index, frame = 1, 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        keys[event[2]]:set_value(event[3])
        machine:logerror(string.format('nsb1_sms_%s_contact: key=%s down=%d t=%.3f physical_only=1\n',
            reload and 'reload' or 'draft',event[2],event[3],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        local label = _G.nsb1_sms_empty and 'empty' or (reload and 'reload' or 'draft')
        machine.screens[':screen']:snapshot(string.format('sms_%s_%d.png',label,frame))
        frame = frame+1
    end
end,'NSB-1 offline physical SMS draft')
