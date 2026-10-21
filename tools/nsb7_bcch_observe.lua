-- Own NSB-7 BCCH consumer observations; no guest writes.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_radio_measurement_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
for _,pc in ipairs({0x2e4e96,0x29d42c}) do
    local count=0
    local aligned=pc-pc%4
    taps[#taps+1]=memory:install_read_tap(aligned,aligned+3,'nsb7_bcch_'..pc,function()
        if cpu.state['R15'].value~=pc or count>=160 then return end
        local object=pc==0x2e4e96 and cpu.state['R0'].value or memory:read_u32(0x12d7ac)
        if object<0x100000 or object>=0x17ff00 then return end
        count=count+1
        local body={}
        for index=0,23 do body[#body+1]=string.format('%02x',memory:read_u8(object+14+index)) end
        machine:logerror(string.format('nsb7_bcch: pc=%08x task=%u tag=%04x mode=%02x error=%02x channel=%04x block=%s flags=%02x t=%.9f observer_writes=0\n',
            pc,memory:read_u8(0x100022),memory:read_u16(object),memory:read_u8(object+4),
            memory:read_u8(object+6),memory:read_u16(object+10),table.concat(body),
            memory:read_u8(0x12d791),machine.time:as_double()))
    end)
end
taps[#taps+1]=memory:install_write_tap(0x12d790,0x12d793,'nsb7_bcch_flags',function(_,value,mask)
    if mask&0x00ff0000==0 then return end
    machine:logerror(string.format('nsb7_bcch_flags: flags=%02x pc=%08x t=%.9f observer_writes=0\n',
        (value>>16)&255,cpu.state['R15'].value,machine.time:as_double()))
end)
_G.nsb7_bcch_taps=taps
