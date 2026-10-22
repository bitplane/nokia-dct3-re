-- Physical authenticated Change PIN2 refusal and subsequent menu navigation.
_G.nsm1_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{4,4,6},
    {1,2,0x19},{1,8,0x18},{1,8,0x18},{1,2,0x19}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsm1_pin_menu.lua')
local machine=manager.machine
local field
for _,candidate in pairs(machine.ioport.ports[':COL.1'].fields) do
    if candidate.mask==8 then field=candidate end
end
assert(field,'Down contact unavailable')
local previous=false
local captured={}
emu.register_frame_done(function()
    local time=machine.time:as_double()
    local down=time>=34 and time<34.2
    if down~=previous then
        field:set_value(down and 1 or 0)
        machine:logerror(string.format('nsm1_puk2_permanent_contact: key=18 down=%d t=%.6f firmware_writes=0\n',down and 1 or 0,time))
        previous=down
    end
    for _,second in ipairs({32,38,47}) do
        if time>=second and not captured[second] then
            machine.screens[':screen']:snapshot('nsm1_puk2_permanent_'..second..'.png')
            captured[second]=true
        end
    end
end)
