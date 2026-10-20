-- Cold Outbox lookup through the product's direct Messages menu.
_G.nsk1_security_code_only = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local keys = {}
for name,cell in pairs({menu={1,4},two={3,2}}) do
    for _,field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask == cell[2] then keys[name] = field end
    end
    assert(keys[name],'own Outbox contact missing')
end
local events = {}
local sequence = {{21,'menu'},{23,'menu'},{25,'two'},{27,'two'},{31,'menu'},{34,'menu'}}
for index,event in ipairs(sequence) do
    events[#events+1] = {event[1],event[2],1,index}
    events[#events+1] = {event[1]+0.2,event[2],0,index}
end
local index,frame = 1,1
local times = _G.nsk1_sms_empty and {35,38,40} or {30,32,36}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        keys[event[2]]:set_value(event[3])
        machine:logerror(string.format('nsk1_sms_reload_contact: index=%d key=%s down=%d t=%.9f physical_only=1\n',event[4],event[2],event[3],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot((_G.nsk1_sms_empty and 'nsk1_sms_empty_' or 'nsk1_sms_reload_')..frame..'.png')
        frame = frame+1
    end
end,'NSK-1 physical cold Outbox read')
