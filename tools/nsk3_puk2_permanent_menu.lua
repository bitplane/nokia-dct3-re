-- Select the refused PIN2 item, then move through the live security menu.
_G.nsk3_pin2_prompt_only=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_pin2_change.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
assert(memory:read_u8(0x2c7a30+3*5+1)==0x18,'own Down contact changed')
local field
for _,candidate in pairs(machine.ioport.ports[':COL.1'].fields) do
    if candidate.mask==8 then field=candidate end
end
assert(field,'own Down contact unavailable')
local edge=1
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if edge<=2 and now>=(edge==1 and 34 or 34.2) then
        local down=edge==1 and 1 or 0
        field:set_value(down)
        machine:logerror(string.format('nsk3_puk2_permanent_contact: key=18 down=%d t=%.6f firmware_writes=0\n',down,now))
        edge=edge+1
    end
end)
