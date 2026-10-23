_G.nse2_pin2_prompt_only=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_pin2_fixture.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local space=cpu.spaces.program
local decoded=0
_G.nse2_permanent_decode_tap=space:install_read_tap(0x2b382c,0x2b3847,'nse2_permanent_decode',function(offset,value,mask)
    local now=machine.time:as_double()
    if cpu.state['R15'].value~=0x299044 or now<28.4 or now>29 or decoded>=8 then return end
    for lane=0,3 do
        local shift=(3-lane)*8
        if mask & (255<<shift)~=0 then
            decoded=decoded+1
            machine:logerror(string.format('nse2_permanent_decode: raw=%d key=%02x t=%.9f observer_writes=0\n',
                offset+lane-0x2b382c,(value>>shift)&255,now))
        end
    end
end)
local field
for _,candidate in pairs(machine.ioport.ports[':COL.0'].fields) do
    if candidate.mask==8 then field=candidate end
end
assert(field,'NSE-2 Down contact missing')
local edge=1
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if edge>2 or now<(edge==1 and 28.5 or 28.65) then return end
    local down=edge==1 and 1 or 0
    field:set_value(down)
    machine:logerror(string.format('nse2_permanent_contact: down=%d t=%.9f physical_only=1\n',down,now))
    edge=edge+1
end,'NSE-2 physical navigation after permanent PIN2 refusal')
