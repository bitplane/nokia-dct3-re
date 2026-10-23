-- Observe the receive activity counter before type classification and at expiry.
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local space=cpu.spaces.program
_G.nse2_receive_activity_taps={}
for _,site in ipairs({{0x29909e,'packet'},{0x2991b8,'expiry'}}) do
    local address,kind=site[1],site[2]
    local count=0
    _G.nse2_receive_activity_taps[#_G.nse2_receive_activity_taps+1]=space:install_read_tap(address & ~3,(address & ~3)+3,'nse2_receive_activity',function()
        if cpu.state['R15'].value~=address then return end
        count=count+1
        if count>64 then return end
        local base=cpu.state['R4'].value
        local message=cpu.state['R5'].value
        machine:logerror(string.format('nse2_receive_activity: kind=%s n=%d base=%08x count=%04x message=%08x type=%02x t=%.9f observer_writes=0\n',
            kind,count,base,space:read_u16(base+2),message,kind=='packet' and space:read_u8(message+3) or 0,machine.time:as_double()))
    end)
end
