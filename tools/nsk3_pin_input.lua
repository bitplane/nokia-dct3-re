-- Physical wrong/correct PIN on the explicit shared laboratory card.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_sim_observe.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local correct=_G.nsk3_correct_pin or false
local cells=correct and {{2,2,1},{3,2,2},{4,2,3},{2,4,4},{1,2,0x19}}
    or {{2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19}}
local events={}
for i,cell in ipairs(cells) do
    local row=cell[2]==2 and 1 or 2
    assert(memory:read_u8(0x2c7a30+row*5+cell[1])==cell[3])
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'own PIN contact unavailable')
    events[#events+1]={9+(i-1)*0.5,field,1,cell[3]}
    events[#events+1]={9.2+(i-1)*0.5,field,0,cell[3]}
end
local next_event,next_frame=1,1
local frames={5,12,17}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[next_event] and now>=events[next_event][1] do
        local event=events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk3_pin_contact: correct=%d key=%02x down=%d t=%.6f firmware_writes=0\n',correct and 1 or 0,event[4],event[3],now))
        next_event=next_event+1
    end
    if frames[next_frame] and now>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_pin_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
