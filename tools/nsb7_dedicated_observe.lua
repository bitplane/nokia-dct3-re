-- Observe the own queued dedicated-channel acknowledgement, without writes.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_agch_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
local watched={}
taps[#taps+1]=memory:install_read_tap(0x2e5054,0x2e5057,'nsb7_dedicated_ack',function()
    if cpu.state['R15'].value~=0x2e5054 then return end
    local message=cpu.state['R0'].value
    local descriptor=memory:read_u32(0x130844)
    if message<0x100000 or message>=0x17ff00 or memory:read_u8(message+4)~=1 then return end
    if descriptor<0x100000 or descriptor>=0x17ff00 or descriptor%4~=0 or watched[descriptor] then return end
    if memory:read_u16(descriptor)~=0x402 then return end
    watched[descriptor]=true
    local count=0
    taps[#taps+1]=memory:install_write_tap(descriptor,descriptor+3,'nsb7_dedicated_armed_'..descriptor,function(_,value,mask)
        if mask&255==0 or count>=16 then return end
        -- The allocator reuses this address between registration and calls.
        -- Observe the live ACK consumer, not unrelated writes to a freed slot.
        if cpu.state['R15'].value~=0x2e5086 or memory:read_u32(0x130844)~=descriptor or
                memory:read_u16(descriptor)~=0x402 then return end
        count=count+1
        machine:logerror(string.format('nsb7_dedicated_armed: address=%08x value=%02x pc=%08x task=%u t=%.9f observer_writes=0\n',
            descriptor,value&255,cpu.state['R15'].value,memory:read_u8(0x100022),machine.time:as_double()))
    end)
end)
_G.nsb7_dedicated_taps=taps
for _,pc in ipairs({0x2e5228,0x2e513e}) do
    local count=0
    taps[#taps+1]=memory:install_read_tap(pc-pc%4,pc-pc%4+3,'nsb7_block_request_'..pc,function()
        if cpu.state['R15'].value~=pc or count>=16 then return end
        count=count+1
        machine:logerror(string.format('nsb7_block_request: pc=%08x task=%u selector=%02x queue=%08x t=%.9f observer_writes=0\n',
            pc,memory:read_u8(0x100022),cpu.state['R0'].value&255,memory:read_u32(0x130434),machine.time:as_double()))
    end)
end
