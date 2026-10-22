-- Read-only candidate TX-ring observation; no peer response or firmware write.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_security_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local cursor=0
local count=0
-- Physical power fixtures may reset diagnostic cursor state while rails are off.
-- Keep the capture index monotonic; no guest memory or device state is touched.
_G.nsb7_radio_observer_reset=function()
    cursor=0
    machine:logerror(string.format('nsb7_radio_capture: reset_cursor t=%.9f observer_writes=0\n',machine.time:as_double()))
end
_G.nsb7_radio_tx_tap=memory:install_write_tap(0x100a4,0x100a7,'nsb7_radio_tx',function(_,value,mask)
    if mask&0xffff0000==0 or count>=256 then return end
    local producer=(value>>16)&0xffff
    if producer>=0x52 then return end
    while cursor~=producer and count<256 do
        local header=memory:read_u16(0x10000+cursor*2)
        local length=header>>8
        local words=(length+3)//2
        local available=(producer-cursor)%0x52
        if words>available then
            machine:logerror(string.format('nsb7_radio_capture: invalid_extent cursor=%02x producer=%02x header=%04x observer_writes=0\n',cursor,producer,header))
            cursor=producer
            return
        end
        local bytes={}
        for index=0,length-1 do
            local packed=memory:read_u16(0x10000+((cursor+1+index//2)%0x52)*2)
            bytes[#bytes+1]=string.format('%02x',(index%2==0 and packed>>8 or packed)&0xff)
        end
        count=count+1
        local secret=(header&0xff)==0x14 and length>=10 and bytes[1]~='00'
        machine:logerror(string.format('nsb7_radio_tx: index=%d type=%02x length=%u data=%s cursor=%02x producer=%02x pc=%08x t=%.9f observer_writes=0\n',
            count,header&0xff,length,secret and '<redacted>' or table.concat(bytes),cursor,producer,cpu.state['R15'].value,machine.time:as_double()))
        cursor=(cursor+words)%0x52
    end
end)
local state=coroutine.create(function()
    for _,target in ipairs({3.5,6,14,24}) do
        assert(emu.wait(target-machine.time:as_double()))
        machine:logerror(string.format('nsb7_radio_state: bands=%08x measurement_route=%02x t=%.9f observer_writes=0\n',
            memory:read_u32(0x12e4ec),memory:read_u8(0x130784),machine.time:as_double()))
    end
end)
_G.nsb7_radio_state=state
assert(coroutine.resume(state))
