-- Cold physical recent-number recall, without dialling digits or injecting UI state.
_G.nsk3_followup_contacts={{1,0x10,0x1a}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_liveness_observe.lua')
local machine=manager.machine
local field
for _,candidate in pairs(machine.ioport.ports[':COL.0'].fields) do
    if candidate.mask==4 then field=candidate end
end
assert(field,'own Send contact unavailable')
assert(machine.devices[':maincpu'].spaces.program:read_u8(0x2c7a30+10)==0x0e,'own Send table changed')
local stage=0
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if stage==0 and now>=22 then
        field:set_value(1)
        machine:logerror('nsk3_recent_contact: key=0e down=1 firmware_writes=0\n')
        stage=1
    elseif stage==1 and now>=22.2 then
        field:set_value(0)
        machine:logerror('nsk3_recent_contact: key=0e down=0 firmware_writes=0\n')
        stage=2
    elseif stage==2 and now>=23 then
        machine.screens[':screen']:snapshot('nsk3_recent_call.png')
        stage=3
    end
end)
