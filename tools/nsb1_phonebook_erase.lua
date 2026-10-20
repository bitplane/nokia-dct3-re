-- Physical one-by-one erase, or cold empty-book search; no card writes by Lua.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb1_menu_fixture.lua')
local machine = manager.machine
local empty = _G.nsb1_empty_phonebook == true
local sequence = empty and {{30,'menu'},{32,'menu'}} or
    {{30,'down'},{31,'down'},{32,'down'},{34,'menu'},{36,'menu'},{38,'menu'},{40,'menu'}}
local keys = {}
for name,cell in pairs({down={0,8},menu={1,4}}) do
    for _,field in pairs(machine.ioport.ports[':COL.'..cell[1]].fields) do
        if field.mask == cell[2] then keys[name] = field end
    end
    assert(keys[name],'missing physical erase contact')
end
local events = {}
for index,event in ipairs(sequence) do
    events[#events+1] = {event[1],event[2],1,index}
    events[#events+1] = {event[1]+0.2,event[2],0,index}
end
local index,frame = 1,1
local times = empty and {33} or {39,41}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        keys[event[2]]:set_value(event[3])
        machine:logerror(string.format('nsb1_phonebook_erase: phase=%s index=%d key=%s down=%d t=%.9f physical_only=1\n',
            empty and 'cold' or 'erase',event[4],event[2],event[3],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot(string.format('nsb1_phonebook_%s_%d.png',empty and 'empty' or 'erase',frame))
        frame = frame+1
    end
end,'NSB-1 physical phonebook deletion')
