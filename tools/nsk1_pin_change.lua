-- Physical ordinary PIN change, distinct from PUK recovery.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_pin_reload.lua')
local machine,keys=manager.machine,{}
for name,cell in pairs({menu={1,4},down={0,8},one={2,2},two={3,2},three={4,2},four={2,4},five={3,4},six={4,4},seven={2,8},eight={3,8}}) do
    for _,field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask==cell[2] then keys[name]=field end
    end
    assert(keys[name])
end
local events,frames={},{}
local function add(time,name,index,phase)
    events[#events+1]={time,name,1,index,phase}
    events[#events+1]={time+0.2,name,0,index,phase}
end
local menu={'menu','menu','down','down','down','menu','down','down','menu','down','down','down','down','menu','down','menu'}
if _G.nsk1_change_pin2 then table.insert(menu,#menu,'down') end
for index,name in ipairs(menu) do
    add(24+index*2,name,index,'menu')
    frames[#frames+1]=24+index*2+0.5
end
local old=_G.nsk1_change_pin2 and {'five','six','seven','eight','menu'} or {'one','two','three','four','menu'}
local replacement=_G.nsk1_change_pin2 and {'eight','seven','six','five','menu'} or {'four','three','two','one','menu'}
if _G.nsk1_restore_pin2 then old,replacement=replacement,old end
for _,phase in ipairs({{60,'old',old},{64,'new',replacement},{68,'confirm',replacement}}) do
    for index,name in ipairs(phase[3]) do add(phase[1]+(index-1)*0.5,name,index,phase[2]) end
end
for _,time in ipairs({63,67,71,73}) do frames[#frames+1]=time end
local event,frame=1,1
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[event] and now>=events[event][1] do
        local step=events[event]
        keys[step[2]]:set_value(step[3])
        machine:logerror(string.format('nsk1_pin_change_contact: phase=%s index=%d key=%s down=%d t=%.9f physical_only=1\n',step[5],step[4],step[2],step[3],now))
        event=event+1
    end
    if frames[frame] and now>=frames[frame] then
        machine.screens[':screen']:snapshot('nsk1_pin_change_'..frame..'.png')
        frame=frame+1
    end
end)
