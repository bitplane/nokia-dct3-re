-- Physical wrong/correct PIN on the explicit shared laboratory card.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_sim_observe.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local correct=_G.nsk3_correct_pin or false
local puk=_G.nsk3_puk_recovery or false
local cells=correct and {{2,2,1},{3,2,2},{4,2,3},{2,4,4},{1,2,0x19}}
    or {{2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19}}
if puk then
    cells={{2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5},{4,4,6},{2,8,7},{3,8,8},{1,2,0x19}}
    if _G.nsk3_wrong_puk then cells[8]={4,8,9} end
    for _=1,2 do
        for _,cell in ipairs({{2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19}}) do cells[#cells+1]=cell end
    end
elseif _G.nsk3_changed_pin then
    cells={{2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19}}
    correct=true
end
local events={}
for i,cell in ipairs(cells) do
    local row=0
    while (1<<row)~=cell[2] and row<5 do row=row+1 end
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
local frames=puk and {5,14,22,24} or {5,12,17}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[next_event] and now>=events[next_event][1] do
        local event=events[next_event]
        event[2]:set_value(event[3])
        if puk then
            machine:logerror(string.format('nsk3_puk_contact: key=%02x down=%d t=%.6f firmware_writes=0\n',event[4],event[3],now))
        else
            machine:logerror(string.format('nsk3_pin_contact: correct=%d key=%02x down=%d t=%.6f firmware_writes=0\n',correct and 1 or 0,event[4],event[3],now))
        end
        next_event=next_event+1
    end
    if frames[next_frame] and now>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_pin_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
