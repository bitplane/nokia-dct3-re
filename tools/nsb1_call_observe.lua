-- Bounded physical ordinary-call probe; no RF or registration responses.
-- Retire detailed ring taps once native search-result ownership is accepted.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_security_fixture.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local memory = cpu.spaces['program']
if machine.debugger then
    machine.debugger:command('focus :maincpu')
    machine.debugger:command('temp9=0')
    machine.debugger:command('temp8=0')
    machine.debugger:command('bpset 268944,r0==c&&r1>=100000&&r1<110000&&w@r1==13b8&&temp9<10,{temp9=temp9+1;logerror "nsb1_call_result_post: task=%08x object=%08x header=%08x caller=%08x observer_writes=0\\n",r0,r1,d@r1,r14;g}')
    for _, address in ipairs({'20f63c','20f68e','20f6b8','213a28','213a68','2143e0','210bd4'}) do
        machine.debugger:command('bpset ' .. address .. ',temp8<20,{temp8=temp8+1;logerror "nsb1_call_search_stage: pc=%08x r0=%08x r4=%08x state=%02x phase=%02x cycles=%016x observer_writes=0\\n",pc,r0,r4,b@106a9c,b@106aa1,totalcycles;g}')
    end
end
local observing = false
local function shared_read(address)
    observing = true
    local value = memory:read_u16(address)
    observing = false
    return value
end
local keys = {}
for name, definition in pairs({menu={1,4},five={3,4},zero={3,0x10},one={2,2}}) do
    for _, field in pairs(machine.ioport.ports[':COL.' .. definition[1]].fields) do
        if field.mask == definition[2] then keys[name] = field end
    end
    assert(keys[name], 'own call contact missing: ' .. name)
end
local events = {}
for index, name in ipairs({'five','five','five','zero','one','zero','zero','menu'}) do
    local when = index == 8 and 32 or 26+(index-1)*0.6
    events[#events+1] = {when,name,1}
    events[#events+1] = {when+0.2,name,0}
end
-- The same softkey is labelled Call while dialing and End in this context.
events[#events+1] = {52,'menu',1}
events[#events+1] = {52.2,'menu',0}
local index, frame = 1, 1
local times = {30.5,33,36,50,59}
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while index <= #events and now >= events[index][1] do
        local event = events[index]
        keys[event[2]]:set_value(event[3])
        machine:logerror(string.format('nsb1_call_contact: key=%s down=%d t=%.9f physical_only=1\n',event[2],event[3],now))
        index = index+1
    end
    if frame <= #times and now >= times[frame] then
        machine.screens[':screen']:snapshot(string.format('nsb1_call_%d.png',frame))
        frame = frame+1
    end
end,'NSB-1 ordinary call boundary')
local commits, previous_producer = 0, 0
_G.nsb1_call_tx = memory:install_write_tap(0x100a4,0x100a7,'nsb1_call_tx',function(offset,data,mask)
    if mask & 0xffff0000 == 0 then return end
    -- The tap observes the completed write; retain the previous observed cursor.
    local old, new = previous_producer, (data >> 16) & 0xffff
    previous_producer = new
    if machine.time:as_double() < 1 or commits >= 32 then return end
    if old >= 0x52 or new >= 0x52 or old == new then return end
    commits = commits+1
    local cursor, count = old, 0
    while cursor ~= new and count < 0x52 do
        local header = shared_read(0x10000+2*cursor)
        local length, payload = header >> 8, {}
        local words = (length+3)//2
        local available = (new-cursor)%0x52
        if words > available or words == 0 then break end
        for byte=0,length-1 do
            local value = memory:read_u16(0x10000+2*((cursor+1+byte//2)%0x52))
            payload[#payload+1] = string.format('%02x',byte%2 == 0 and value >> 8 or value & 255)
        end
        machine:logerror(string.format('nsb1_call_tx: type=%02x length=%d data=%s old=%02x new=%02x pc=%08x t=%.9f observer_writes=0\n',
            header & 255,length,table.concat(payload),old,new,cpu.state['R15'].value,machine.time:as_double()))
        cursor, count = (cursor+words)%0x52, count+words
    end
end)
local result_reads = 0
local status_reads = 0
local state_writes = 0
_G.nsb1_call_search_state = memory:install_write_tap(0x106a9c,0x106a9f,'nsb1_call_search_state',function(offset,data,mask)
    if mask & 0xff000000 ~= 0 and state_writes < 24 then
        state_writes = state_writes+1
        machine:logerror(string.format('nsb1_call_search_state: value=%02x pc=%08x caller=%08x t=%.9f observer_writes=0\n',
            (data >> 24) & 255,cpu.state['R15'].value,cpu.state['R14'].value,machine.time:as_double()))
    end
end)
_G.nsb1_call_status = memory:install_read_tap(0x102a48,0x102a4b,'nsb1_call_status',function(offset,data,mask)
    -- Observed fixed-pool object in this fixture, not an allocator closure.
    if not observing and cpu.state['R15'].value ~= 0x268944 and
            machine.time:as_double() >= 40 and status_reads < 8 and
            mask & 0xffff0000 ~= 0 and (data >> 16) & 0xffff == 0x13b8 then
        status_reads = status_reads+1
        machine:logerror(string.format('nsb1_call_status_read: pc=%08x caller=%08x t=%.9f observer_writes=0\n',
            cpu.state['R15'].value,cpu.state['R14'].value,machine.time:as_double()))
    end
end)
_G.nsb1_call_result = memory:install_read_tap(0x10100,0x101c7,'nsb1_call_result',function(offset,data,mask)
    if not observing and machine.time:as_double() >= 40 and result_reads < 8 and
            ((mask & 0xffff0000 ~= 0 and (data >> 16) & 0xffff == 0xa28b) or
             (mask & 0xffff ~= 0 and data & 0xffff == 0xa28b)) then
        result_reads = result_reads+1
        machine:logerror(string.format('nsb1_call_result_read: address=%08x pc=%08x caller=%08x t=%.9f observer_writes=0\n',
            offset,cpu.state['R15'].value,cpu.state['R14'].value,machine.time:as_double()))
    end
end)
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local rx_count, rx_previous = 0, 0x80
_G.nsb1_call_rx = dsp.spaces['data']:install_write_tap(0x08e4,0x08e4,'nsb1_call_rx',function(offset,data,mask)
    local old, new = rx_previous, data & 0xffff
    rx_previous = new
    if machine.time:as_double() < 1 or rx_count >= 64 or old == new or
            old < 0x80 or old >= 0xe4 or new < 0x80 or new >= 0xe4 then return end
    rx_count = rx_count+1
    local cursor, walked = old, 0
    while cursor ~= new and walked < 0x64 do
        local header = shared_read(0x10000+2*cursor)
        local length, payload = header >> 8, {}
        local words = (length+3)//2
        local available = (new-cursor)%0x64
        if words == 0 or words > available then break end
        for byte=0,length-1 do
            local position = 0x80+((cursor-0x80+1+byte//2)%0x64)
            local value = shared_read(0x10000+2*position)
            payload[#payload+1] = string.format('%02x',byte%2 == 0 and value >> 8 or value & 255)
        end
        machine:logerror(string.format('nsb1_call_rx: type=%02x length=%d data=%s pc=%04x t=%.9f observer_writes=0\n',
            header & 255,length,table.concat(payload),dsp.state['PC'].value,machine.time:as_double()))
        cursor, walked = 0x80+((cursor-0x80+words)%0x64), walked+words
    end
end)
local samples = 0
_G.nsb1_call_rf = dsp.spaces['io']:install_read_tap(0x27,0x27,'nsb1_call_rf',function(offset,data,mask)
    if machine.time:as_double() >= 4 and samples < 8 then
        samples = samples+1
        machine:logerror(string.format('nsb1_call_rf: value=%04x pc=%04x t=%.9f observer_writes=0\n',
            data,dsp.state['PC'].value,machine.time:as_double()))
    end
end)
