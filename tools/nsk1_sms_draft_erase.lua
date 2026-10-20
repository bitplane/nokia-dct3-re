-- Physical deletion of the independently persisted and opened draft.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_sms_draft_reload.lua')
local machine = manager.machine
local menu
for _,field in pairs(machine.ioport.ports[':COL.1'].fields) do
    if field.mask == 4 then menu = field end
end
assert(menu,'own SMS erase contact missing')
local events = {{38,1},{38.2,0},{40,1},{40.2,0},{42,1},{42.2,0}}
local index,frame = 1,1
local times = {39,41,43,46,50}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        menu:set_value(event[2])
        machine:logerror(string.format('nsk1_sms_erase_contact: index=%d key=menu down=%d t=%.9f physical_only=1\n',(index+1)//2,event[2],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot('nsk1_sms_erase_'..frame..'.png')
        frame = frame+1
    end
end,'NSK-1 physical SMS deletion')
