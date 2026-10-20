-- Physical default-code entry and menu navigation; all memory access is read-only.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_boot_observe.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
assert(space:read_u32(0x296d08) == 112,'security fixture requires own standard family')
machine.debugger:command('focus :maincpu')
machine.debugger:command('temp1=0')
machine.debugger:command('bpset 28f4e4,temp1<8,{temp1=temp1+1;logerror "nsk1_code_gate: match=%08x retry=%02x cycles=%016x observer_writes=0\\n",r0,b@r1,totalcycles;g}')
local events = {}
for index,contact in ipairs({{2,2},{3,2},{4,2},{2,4},{3,4},{1,4},{1,4},{1,4},{1,4}}) do
    if _G.nsk1_security_code_only and index > 6 then break end
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.'..contact[1]].fields) do
        if candidate.mask == contact[2] then field = candidate end
    end
    assert(field,'own physical security contact missing')
    local time = index <= 6 and 11+index or 21+(index-7)*2
    events[#events+1] = {time,field,1,index,contact[1],contact[2]}
    events[#events+1] = {time+0.2,field,0,index,contact[1],contact[2]}
end
local next_event,next_frame,keys = 1,1,0
local frames = {18,20,22,24,26,28,30}
_G.nsk1_security_key_tap = space:install_read_tap(0x294860,0x294863,'nsk1_code_key',function()
    if cpu.state['R15'].value ~= 0x294862 or keys >= 32 then return end
    keys = keys+1
    machine:logerror(string.format('nsk1_code_key: value=%02x t=%.9f observer_writes=0\n',cpu.state['R0'].value,machine.time:as_double()))
end)
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nsk1_code_contact: index=%d column=%d mask=%02x down=%d t=%.9f physical_only=1\n',
            event[4],event[5],event[6],event[3],now))
        next_event = next_event+1
    end
    if next_frame <= #frames and now >= frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk1_code_'..frames[next_frame]..'s.png')
        machine:logerror(string.format('nsk1_code_state: checklist=%02x root=%02x retry=%02x t=%.9f observer_writes=0\n',
            space:read_u8(0x10be35),space:read_u8(0x10ffb0),space:read_u8(0x10bee8),now))
        next_frame = next_frame+1
    end
end,'NSK-1 physical phone-code and phonebook menu fixture')
