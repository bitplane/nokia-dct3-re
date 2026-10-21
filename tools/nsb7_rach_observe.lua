-- Own random-access completion and request-reference observations, read-only.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_bcch_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
for _,pc in ipairs({0x2e4fa4,0x2f5274}) do
    local count=0
    taps[#taps+1]=memory:install_read_tap(pc,pc+3,'nsb7_rach_'..pc,function()
        if cpu.state['R15'].value~=pc or count>=32 then return end
        local object=cpu.state['R0'].value
        if object<0x100000 or object>=0x17ff00 then return end
        count=count+1
        local data={}
        for index=0,7 do data[#data+1]=string.format('%02x',memory:read_u8(object+4+index)) end
        machine:logerror(string.format('nsb7_rach: pc=%08x task=%u type=%02x payload=%s t=%.9f observer_writes=0\n',
            pc,memory:read_u8(0x100022),memory:read_u8(object+3),table.concat(data),machine.time:as_double()))
    end)
end
local references=0
taps[#taps+1]=memory:install_read_tap(0x2898e8,0x2898eb,'nsb7_rach_reference',function()
    if cpu.state['R15'].value~=0x2898e8 or cpu.state['R0'].value~=17 or references>=32 then return end
    local object=cpu.state['R1'].value
    if object<0x100000 or object>=0x17ff00 or memory:read_u16(object)~=0x7f9 then return end
    references=references+1
    machine:logerror(string.format('nsb7_rach_reference: index=%d task=%u target=17 tag=07f9 t1=%02x t3=%02x t2=%02x ra=%02x t=%.9f observer_writes=0\n',
        references,memory:read_u8(0x100022),memory:read_u8(object+5),memory:read_u8(object+6),
        memory:read_u8(object+7),memory:read_u8(object+8),machine.time:as_double()))
end)
_G.nsb7_rach_taps=taps
