-- Physical settings navigation on the own NSB-1 matrix; no state injection.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') ..
    (_G.nsb1_security_request_action=='on' and 'nsb1_security_fixture.lua' or 'nsb1_pin_reload.lua'))
local machine=manager.machine
local contacts={{1,4},{2,4},{3,2},{2,2}}
if _G.nsb1_security_request_action then
    for _,contact in ipairs({{2,2},{3,2},{4,2},{2,4},{1,4},
        _G.nsb1_security_request_action=='on' and {1,16} or {0,8},{1,4}}) do
        contacts[#contacts+1]=contact
    end
end
local events={}
for index,contact in ipairs(contacts) do
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..contact[1]].fields) do
        if candidate.mask==contact[2] then field=candidate end
    end
    assert(field,'own settings contact missing')
    events[#events+1]={26+(index-1)*2,field,1,index}
    events[#events+1]={26.2+(index-1)*2,field,0,index}
end
local index,frame=1,1
local times={27,29,31,33,35,45,47,49,51}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[index] and now>=events[index][1] do
        local event=events[index]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsb1_settings_contact: index=%d down=%d t=%.9f physical_only=1\n',event[4],event[3],now))
        index=index+1
    end
    if times[frame] and now>=times[frame] then
        machine.screens[':screen']:snapshot('nsb1_security_settings_'..times[frame]..'.png')
        frame=frame+1
    end
end)
