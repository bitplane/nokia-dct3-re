-- Open the persisted draft, then physically choose and confirm Erase.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb1_sms_draft_reload.lua')
local machine = manager.machine
local menu
for _,field in pairs(machine.ioport.ports[':COL.1'].fields) do
    if field.mask == 4 then menu = field end
end
assert(menu,'missing physical SMS erase contact')
local events = {{40,1},{40.2,0},{42,1},{42.2,0},{44,1},{44.2,0}}
local index,frame = 1,1
local times = {43,45}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        menu:set_value(event[2])
        machine:logerror(string.format('nsb1_sms_erase_contact: index=%d down=%d t=%.9f physical_only=1\n',
            (index+1)//2,event[2],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot('sms_erase_'..frame..'.png')
        frame = frame+1
    end
end,'NSB-1 physical SMS erase confirmation')
