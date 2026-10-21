-- Opt-in PCS acquisition peer; read-only MCU observations.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_radio_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
local received=0
taps[#taps+1]=memory:install_read_tap(0x309f24,0x309f27,'nsb7_radio_rx_dispatch',function()
    if cpu.state['R15'].value~=0x309f26 or received>=256 then return end
    local object=cpu.state['R4'].value
    if object<0x100000 or object>=0x17ff00 then return end
    local kind=memory:read_u8(object+3)
    if kind<0x80 then return end
    received=received+1
    machine:logerror(string.format('nsb7_radio_rx: index=%d type=%02x length=%u t=%.9f observer_writes=0\n',
        received,kind,memory:read_u8(object+2),machine.time:as_double()))
end)
for _,pc in ipairs({0x2e5258,0x2a3704}) do
    local count=0
    taps[#taps+1]=memory:install_read_tap(pc,pc+3,'nsb7_measurement_'..pc,function()
        if cpu.state['R15'].value~=pc or count>=32 then return end
        count=count+1
        local object=cpu.state['R0'].value
        if object<0x100000 or object>=0x17ff00 then return end
        local rssi=memory:read_u8(object+9)
        if rssi>=128 then rssi=rssi-256 end
        machine:logerror(string.format('nsb7_measurement: pc=%08x task=%u type=%02x first=%04x second=%04x rssi=%d t=%.9f observer_writes=0\n',
            pc,memory:read_u8(0x100022),memory:read_u8(object+3),memory:read_u16(object+6),
            memory:read_u16(object+10),rssi,machine.time:as_double()))
    end)
end
local channel_state=coroutine.create(function()
    for _,target in ipairs({6,14,24}) do
        assert(emu.wait(target-machine.time:as_double()))
        machine:logerror(string.format('nsb7_channel_state: cached=%08x channel=%04x mode=%02x bsic=%02x t=%.9f observer_writes=0\n',
            memory:read_u32(0x1306e4),memory:read_u16(0x13030c),memory:read_u8(0x130310),
            memory:read_u8(0x13030f),machine.time:as_double()))
    end
end)
_G.nsb7_channel_state=channel_state
assert(coroutine.resume(channel_state))
for _,pc in ipairs({0x2e5054}) do
    local count=0
    taps[#taps+1]=memory:install_read_tap(pc,pc+3,'nsb7_channel_ack_'..pc,function()
        if cpu.state['R15'].value~=pc or count>=16 then return end
        count=count+1
        local object=cpu.state['R0'].value
        if object<0x100000 or object>=0x17ff00 then return end
        machine:logerror(string.format('nsb7_channel_ack: pc=%08x task=%u type=%02x bit=%02x cached=%08x channel=%04x mode=%02x bsic=%02x t=%.9f observer_writes=0\n',
            pc,memory:read_u8(0x100022),memory:read_u8(object+3),memory:read_u8(object+4),
            memory:read_u32(0x1306e4),memory:read_u16(0x13030c),memory:read_u8(0x130310),
            memory:read_u8(0x13030f),machine.time:as_double()))
    end)
end
local classes=0
local candidates=0
taps[#taps+1]=memory:install_read_tap(0x2b6880,0x2b6883,'nsb7_candidate_before_transport',function()
    if cpu.state['R15'].value~=0x2b6880 or candidates>=16 then return end
    local object=cpu.state['R0'].value
    if object<0x100000 or object>=0x17ff00 then return end
    candidates=candidates+1
    local words={}
    for index=0,79 do
        words[#words+1]=string.format('%04x',memory:read_u16(object+4+index*2))
    end
    machine:logerror(string.format('nsb7_candidate_object: index=%d type=%02x length=%u words=%s t=%.9f observer_writes=0\n',
        candidates,memory:read_u8(object+3),memory:read_u8(object+2),table.concat(words,','),machine.time:as_double()))
    machine:logerror(string.format('nsb7_candidate_context: route=%02x root08=%08x root0c=%08x root18=%08x root28=%08x t=%.9f observer_writes=0\n',
        memory:read_u8(0x130784),memory:read_u32(0x12fbd0),memory:read_u32(0x12fbd4),
        memory:read_u32(0x12fbe0),memory:read_u32(0x12fbf0),machine.time:as_double()))
    local record=memory:read_u32(0x12fbe0)
    if record>=0x100000 and record<0x17fff0 then
        machine:logerror(string.format('nsb7_candidate_record: address=%08x channel=%04x flags=%02x t=%.9f observer_writes=0\n',
            record,memory:read_u16(record+6),memory:read_u8(record+13),machine.time:as_double()))
    end
end)
taps[#taps+1]=memory:install_read_tap(0x2c7fcc,0x2c7fcf,'nsb7_measured_channel_class',function()
    if cpu.state['R15'].value~=0x2c7fcc or machine.time:as_double()<3.4 or classes>=32 then return end
    local channel=cpu.state['R5'].value
    if channel<512 or channel>810 then return end
    classes=classes+1
    machine:logerror(string.format('nsb7_measurement_class: channel=%04x class=%u t=%.9f observer_writes=0\n',
        channel,cpu.state['R4'].value,machine.time:as_double()))
end)
_G.nsb7_measurement_taps=taps
local commits=0
taps[#taps+1]=memory:install_write_tap(0x100a4,0x100a7,'nsb7_sch_candidate_commit',function()
    if cpu.state['R15'].value~=0x2cb0ec or machine.time:as_double()<3.4 or commits>=128 then return end
    local record=memory:read_u32(0x12fbe0)
    commits=commits+1
    if record>=0x100000 and record<0x17fff0 then
        machine:logerror(string.format('nsb7_sch_candidate_commit: channel=%04x flags=%02x bsic=%02x t=%.9f observer_writes=0\n',
            memory:read_u16(record+6),memory:read_u8(record+13),memory:read_u8(record+10),machine.time:as_double()))
    end
    local pending=memory:read_u32(0x130844)
    local cached=memory:read_u32(0x1306e4)
    if cached>=0x100000 and cached<0x17ff00 then
        local bytes={}
        for index=0,11 do bytes[#bytes+1]=string.format('%02x',memory:read_u8(cached+index)) end
        machine:logerror(string.format('nsb7_channel_cached: descriptor=%08x data=%s live_channel=%04x live_mode=%02x t=%.9f observer_writes=0\n',
            pending,table.concat(bytes),memory:read_u16(0x13030c),memory:read_u8(0x130310),machine.time:as_double()))
    end
    if pending>=0x100000 and pending<0x17ff00 then
        machine:logerror(string.format('nsb7_channel_pending: address=%08x tag=%04x bit=%02x armed=%02x t=%.9f observer_writes=0\n',
            pending,memory:read_u16(pending),memory:read_u8(pending+2),memory:read_u8(pending+3),machine.time:as_double()))
    end
end)
for _,pc in ipairs({0x2e4ef4,0x287574,0x28734c}) do
    local count=0
    local aligned=pc-pc%4
    taps[#taps+1]=memory:install_read_tap(aligned,aligned+3,'nsb7_sch_'..pc,function()
        if cpu.state['R15'].value~=pc or count>=16 then return end
        count=count+1
        local record=memory:read_u32(0x12fbd0)
        local flags=0xff
        if record>=0x100000 and record<0x17fff0 then flags=memory:read_u8(record+13) end
        local payload={}
        local object=cpu.state['R0'].value
        if object>=0x100000 and object<0x17ff00 and memory:read_u8(object+4)~=0x40 then return end
        if object>=0x100000 and object<0x17ff00 then
            for index=0,13 do payload[#payload+1]=string.format('%02x',memory:read_u8(object+4+index)) end
        end
        machine:logerror(string.format('nsb7_sch: pc=%08x task=%u selected=%08x flags=%02x payload=%s t=%.9f observer_writes=0\n',
            pc,memory:read_u8(0x100022),record,flags,table.concat(payload),machine.time:as_double()))
    end)
end
