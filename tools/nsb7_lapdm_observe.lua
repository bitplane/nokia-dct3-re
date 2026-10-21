-- Dedicated decoded-block observations at both MCU consumer boundaries.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_dedicated_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
for _,pc in ipairs({0x2e49e6,0x2645ec}) do
    local count=0
    local aligned=pc-pc%4
    taps[#taps+1]=memory:install_read_tap(aligned,aligned+3,'nsb7_lapdm_'..pc,function()
        if cpu.state['R15'].value~=pc or count>=32 then return end
        local object=cpu.state['R0'].value
        if object<0x100000 or object>=0x17ff00 then return end
        local selector=memory:read_u8(object+(pc==0x2e49e6 and 4 or 11))
        if selector~=0x80 and selector~=0xb0 then return end
        count=count+1
        local body={}
        for index=0,23 do body[#body+1]=string.format('%02x',memory:read_u8(object+14+index)) end
        machine:logerror(string.format('nsb7_lapdm_downlink: pc=%08x task=%u selector=%02x block=%s t=%.9f observer_writes=0\n',
            pc,memory:read_u8(0x100022),selector,table.concat(body),machine.time:as_double()))
    end)
end
_G.nsb7_lapdm_taps=taps
local contexts={}
taps[#taps+1]=memory:install_read_tap(0x2664bc,0x2664bf,'nsb7_ua_contention',function()
    if cpu.state['R15'].value~=0x2664be then return end
    local context=memory:read_u32(cpu.state['R13'].value+0x10)
    if context<0x100000 or context>=0x17fe00 or contexts[context] then return end
    contexts[context]=true
    local length=memory:read_u16(context+0x12e)
    local pending=memory:read_u32(context+0x130)
    if length>23 or pending<0x100000 or pending>=0x17ff00 then return end
    local sent,received={},{}
    for index=0,length-1 do
        sent[#sent+1]=string.format('%02x',memory:read_u8(pending+4+index))
        received[#received+1]=string.format('%02x',memory:read_u8(context+0x1a+index))
    end
    machine:logerror(string.format('nsb7_ua_compare: context=%08x state=%u length=%u sent=%s received=%s t=%.9f observer_writes=0\n',
        context,memory:read_u8(context+0x163),length,table.concat(sent),table.concat(received),machine.time:as_double()))
    if context%4~=0 then return end
    local count=0
    taps[#taps+1]=memory:install_write_tap(context+0x160,context+0x163,'nsb7_lapdm_state_'..context,function(_,value,mask)
        if mask&255==0 or count>=16 then return end
        count=count+1
        machine:logerror(string.format('nsb7_lapdm_state: context=%08x state=%u pc=%08x task=%u t=%.9f observer_writes=0\n',
            context,value&255,cpu.state['R15'].value,memory:read_u8(0x100022),machine.time:as_double()))
    end)
end)
