-- Matched physical dial/Call/End observation; no firmware or MMIO writes.
local source=debug.getinfo(1,'S').source:sub(2)
_G.nsk3_followup_contacts={{1,0x10,0x1a},{2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5}}
dofile(source:match('^(.*[/\\])') .. 'nsk3_liveness_observe.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local events={}
if _G.nsk3_radio_call~=false then
    for _,key in ipairs({{22,4,0x0e},{30,8,0x0f}}) do
        local field
        for _,candidate in pairs(machine.ioport.ports[':COL.0'].fields) do
            if candidate.mask==key[2] then field=candidate end
        end
        assert(field,'own call contact unavailable')
        local row=key[2]==4 and 2 or 3
        assert(memory:read_u8(0x2c7a30+row*5)==key[3],'own Send/End table changed')
        events[#events+1]={key[1],field,1,key[3]}
        events[#events+1]={key[1]+0.2,field,0,key[3]}
    end
end
local next_event=1
emu.register_frame_done(function()
    local now=machine.time:as_double()
    while events[next_event] and now>=events[next_event][1] do
        local event=events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk3_radio_contact: key=%02x down=%d t=%.6f firmware_writes=0\n',event[4],event[3],now))
        next_event=next_event+1
    end
end)
