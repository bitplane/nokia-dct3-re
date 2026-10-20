-- Physical wrong/correct CHV1 entry; separate from the phone security code.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_sim_observe.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local correct=_G.nsm1_correct_pin or false
local puk=_G.nsm1_puk_recovery or false
local cells=correct and {{2,2,1},{3,2,2},{4,2,3},{2,4,4},{1,2,0x19}}
    or {{2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19}}
if puk then
    cells={{2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5},{4,4,6},{2,8,7},{3,8,8},{1,2,0x19}}
    if _G.nsm1_wrong_puk then cells[8]={4,8,9} end
    for _=1,2 do
        for _,cell in ipairs({{2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19}}) do
            cells[#cells+1]=cell
        end
    end
elseif _G.nsm1_changed_pin then
    cells={{2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19}}
    correct=true
end
local events={}
for index,cell in ipairs(cells) do
    local row=0
    while (1<<row)~=cell[2] do row=row+1 end
    assert(memory:read_u8(0x2e0b18+row*5+cell[1])==cell[3],'own PIN matrix changed')
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field)
    events[#events+1]={9+(index-1)*0.5,field,1,cell[3]}
    events[#events+1]={9.2+(index-1)*0.5,field,0,cell[3]}
end
local index,snap=1,1
local times=puk and {5,14,22,24} or {5,12,17}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[index] and now>=events[index][1] do
        local event=events[index];event[2]:set_value(event[3])
        if puk then
            machine:logerror(string.format('nsm1_puk_contact: key=%02x down=%d t=%.6f firmware_writes=0\n',event[4],event[3],now))
        else
            machine:logerror(string.format('nsm1_pin_contact: correct=%d key=%02x down=%d t=%.6f firmware_writes=0\n',correct and 1 or 0,event[4],event[3],now))
        end
        index=index+1
    end
    if times[snap] and now>=times[snap] then
        machine.screens[':screen']:snapshot(string.format('nsm1_pin_%d.png',times[snap]))
        snap=snap+1
    end
end)
