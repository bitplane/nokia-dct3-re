-- Own AGCH routing observations; no guest writes or message injection.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_rach_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
local receives=0
taps[#taps+1]=memory:install_read_tap(0x289b3c,0x289b3f,'nsb7_rr_receive',function()
    if cpu.state['R15'].value~=0x289b3c or memory:read_u8(0x100022)~=14 or receives>=32 then return end
    receives=receives+1
    machine:logerror(string.format('nsb7_rr_receive: index=%d lr=%08x task=14 t=%.9f observer_writes=0\n',
        receives,cpu.state['R14'].value,machine.time:as_double()))
end)
local blocks=0
taps[#taps+1]=memory:install_read_tap(0x2e49e4,0x2e49e7,'nsb7_agch_forward',function()
    if cpu.state['R15'].value~=0x2e49e6 or blocks>=32 then return end
    local object=cpu.state['R0'].value
    if object<0x100000 or object>=0x17ff00 then return end
    blocks=blocks+1
    local body={}
    for index=0,23 do body[#body+1]=string.format('%02x',memory:read_u8(object+14+index)) end
    local frame=memory:read_u8(object+7)*65536+memory:read_u8(object+8)*256+memory:read_u8(object+9)
    machine:logerror(string.format('nsb7_agch_forward: index=%d task=%u mode=%02x error=%02x frame=%u block=%s t=%.9f observer_writes=0\n',
        blocks,memory:read_u8(0x100022),memory:read_u8(object+4),memory:read_u8(object+6),
        frame,table.concat(body),machine.time:as_double()))
end)
local decoded=0
taps[#taps+1]=memory:install_read_tap(0x2645ec,0x2645ef,'nsb7_agch_task14',function()
    if cpu.state['R15'].value~=0x2645ec or memory:read_u8(0x100022)~=14 or decoded>=32 then return end
    local object=cpu.state['R0'].value
    if object<0x100000 or object>=0x17ff00 or memory:read_u16(object)~=0x623 then return end
    decoded=decoded+1
    local body={}
    for index=0,23 do body[#body+1]=string.format('%02x',memory:read_u8(object+14+index)) end
    machine:logerror(string.format('nsb7_agch_consumer: index=%d task=14 tag=0623 mode=%02x block=%s t=%.9f observer_writes=0\n',
        decoded,memory:read_u8(object+11),table.concat(body),machine.time:as_double()))
end)
local results=0
taps[#taps+1]=memory:install_read_tap(0x2e6184,0x2e6187,'nsb7_rr_decoder_result',function()
    if cpu.state['R15'].value~=0x2e6184 or memory:read_u8(0x100022)~=14 or results>=32 then return end
    local block=cpu.state['R2'].value
    if block<0x100000 or block>=0x17ff00 then return end
    results=results+1
    local body={}
    for index=0,23 do body[#body+1]=string.format('%02x',memory:read_u8(block+index)) end
    machine:logerror(string.format('nsb7_rr_decoder_result: index=%d task=14 code=%04x block=%s t=%.9f observer_writes=0\n',
        results,cpu.state['R0'].value&65535,table.concat(body),machine.time:as_double()))
end)
_G.nsb7_agch_taps=taps
