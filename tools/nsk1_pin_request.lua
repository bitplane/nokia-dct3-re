-- Physical Security settings / PIN code request; no firmware-state writes.
local source = debug.getinfo(1,'S').source:sub(2)
local directory = source:match('^(.*[/\\])')
if _G.nsk1_pin_request_enable then
    _G.nsk1_security_code_only=true
    dofile(directory .. 'nsk1_security_fixture.lua')
else
    dofile(directory .. 'nsk1_pin_reload.lua')
end
local machine,keys = manager.machine,{}
for name,cell in pairs({menu={1,4},down={0,8},one={2,2},two={3,2},three={4,2},four={2,4}}) do
    for _,field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask == cell[2] then keys[name]=field end
    end
    assert(keys[name],'physical PIN-request contact missing')
end
local sequence = {'menu','menu','down','down','down','menu','down','down','menu','menu','one','two','three','four','menu','down','menu'}
-- Without startup SIM authentication the first Menu already enters the root.
if _G.nsk1_pin_request_enable then table.remove(sequence,1) end
local events,frames = {},{}
for index,name in ipairs(sequence) do
    local time = 24+index*2
    events[#events+1]={time,name,1,index}
    events[#events+1]={time+0.2,name,0,index}
    frames[#frames+1]=time+0.5
end
local event,frame=1,1
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[event] and now>=events[event][1] do
        local step=events[event]
        keys[step[2]]:set_value(step[3])
        machine:logerror(string.format('nsk1_pin_request_contact: index=%d key=%s down=%d t=%.9f physical_only=1\n',step[4],step[2],step[3],now))
        event=event+1
    end
    if frames[frame] and now>=frames[frame] then
        machine.screens[':screen']:snapshot('nsk1_pin_request_'..frame..'.png')
        frame=frame+1
    end
end)
