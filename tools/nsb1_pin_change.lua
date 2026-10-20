-- Physical Access codes/Change PIN; only firmware may issue card commands.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_pin_reload.lua')
local machine=manager.machine
local cells={
    {1,4},{2,4},{3,2},{3,4},{2,2}, -- Menu4-2-5-1
    {2,2},{3,2},{4,2},{2,4},{1,4}, -- Current1234
    {2,4},{4,2},{3,2},{2,2},{1,4}, -- New4321
    {2,4},{4,2},{3,2},{2,2},{1,4}  -- Confirm4321
}
if _G.nsb1_pin2_change then
    cells={{1,4},{2,4},{3,2},{3,4},{3,2}} -- Own Menu4-2-5-2
    local digits={[1]={2,2},[2]={3,2},[3]={4,2},[4]={2,4},
        [5]={3,4},[6]={4,4},[7]={2,8},[8]={3,8}}
    local old=_G.nsb1_pin2_puk and {8,7,6,5,4,3,2,1} or
        (_G.nsb1_pin2_restore and {8,7,6,5} or {5,6,7,8})
    if _G.nsb1_pin2_wrong_puk then old={8,7,6,5,4,3,2,2} end
    local new=_G.nsb1_pin2_restore and {5,6,7,8} or {8,7,6,5}
    for _,number in ipairs({old,new,new}) do
        for _,digit in ipairs(number) do cells[#cells+1]=digits[digit] end
        cells[#cells+1]={1,4}
    end
    if _G.nsb1_pin2_wrong then cells[9]={3,4} end -- Current5675, not5678.
end
if _G.nsb1_pin2_entry_only then
    while #cells>5 do table.remove(cells) end
    cells[#cells+1]={1,4}
    if _G.nsb1_pin2_entry_back then
        cells[#cells+1]={1,2} -- Distinguish an unselectable item from dead input.
    end
end
local events={}
for index,cell in ipairs(cells) do
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if candidate.mask==cell[2] then field=candidate end
    end
    assert(field,'own PIN-change contact missing')
    events[#events+1]={26+(index-1)*2,field,1,index}
    events[#events+1]={26.2+(index-1)*2,field,0,index}
end
local index,frame=1,1
local times={33,35,37,39,45,55,65,67,75,77}
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[index] and now>=events[index][1] do
        local event=events[index]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsb1_pin_change_contact: index=%d down=%d t=%.9f physical_only=1\n',event[4],event[3],now))
        index=index+1
    end
    if times[frame] and now>=times[frame] then
        machine.screens[':screen']:snapshot('nsb1_pin_change_'..times[frame]..'.png')
        frame=frame+1
    end
end)
