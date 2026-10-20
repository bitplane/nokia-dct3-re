-- Own NSB-1 Menu4-2-4 and physical PIN2; standard card-side FDN only.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_pin_reload.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local tx=0
_G.nsb1_fdn_tx_tap=cpu.spaces.program:install_write_tap(0x20034,0x20037,'nsb1_fdn_tx',function(_,value,mask)
    if mask & 0xff00==0 then return end
    tx=tx+1
    if tx<=4096 then
        machine:logerror(string.format('nsb1_sim_tx: index=%d value=%02x t=%.9f observer_writes=0\n',tx,(value>>8)&0xff,machine.time:as_double()))
    end
end)
-- The observed second Select opens options; the first leaves the summary visible.
local cells={{1,4},{2,4},{3,2},{2,4},{1,4},{1,4},
    _G.nsb1_fdn_disable and {0,8} or {1,16},{1,4},
    {3,4},{4,4},{2,8},{3,8},{1,4}}
if _G.nsb1_fdn_observe then
    while #cells>4 do table.remove(cells) end
end
cells=_G.nsb1_fdn_contacts or cells
local events={}
for index,cell in ipairs(cells) do
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'own FDN contact missing')
    events[#events+1]={26+(index-1)*2,field,1,index}
    local duration=index==#cells and _G.nsb1_fdn_hold_last or 0.2
    events[#events+1]={26+(index-1)*2+duration,field,0,index}
end
local index,frame=1,1
local times=_G.nsb1_fdn_frames or {33,35,37,39,41,49,51,53}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[index] and now>=events[index][1] do
        local event=events[index]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsb1_fdn_contact: index=%d down=%d t=%.9f physical_only=1\n',event[4],event[3],now))
        index=index+1
    end
    if times[frame] and now>=times[frame] then
        machine.screens[':screen']:snapshot('nsb1_fdn_'..times[frame]..'.png')
        frame=frame+1
    end
end)
