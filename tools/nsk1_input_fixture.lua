-- Own key-table-derived physical contacts. No logical events or guest writes.
local source = debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_boot_observe.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
assert(machine.debugger, 'physical input probe requires debugger observations')
local shift = space:read_u32(0x296d08) == 112 and 0 or -0x10
local checklist = space:read_u32(0x25bfb0+shift)
local layout = space:read_u32(0x29494c+shift)
local ready_flags = space:read_u32(0x241898+shift)
assert(checklist >= 0x100000 and checklist < 0x110000)
assert(layout >= 0x100000 and layout < 0x110000)
assert(ready_flags >= 0x100000 and ready_flags+11 <= 0x110000)
machine.debugger:command('focus :maincpu')
machine.debugger:command('temp3=0')
machine.debugger:command('temp4=0')
machine.debugger:command('temp5=0')
machine.debugger:command('temp6=0')
machine.debugger:command(string.format('bpset %x,temp3<20&&r0!=3e,{temp3=temp3+1;logerror "nsk1_key_forward: key=%%02x caller=%%08x cycles=%%016x observer_writes=0\\n",r0,r14,totalcycles;g}',0x294862+shift))
machine.debugger:command(string.format('bpset %x,temp4<40&&r0==1,{temp4=temp4+1;logerror "nsk1_task1_scalar: value=%%08x caller=%%08x cycles=%%016x observer_writes=0\\n",r1,r14,totalcycles;g}',0x2553b8+shift))
machine.debugger:command(string.format('bpset %x,temp5<8,{temp5=temp5+1;logerror "nsk1_checklist_compare: bits=%%02x cycles=%%016x observer_writes=0\\n",r0,totalcycles;g}',0x25bca4+shift))
machine.debugger:command(string.format('bpset %x,temp6<32,{temp6=temp6+1;logerror "nsk1_ready_input: value=%%02x caller=%%08x cycles=%%016x observer_writes=0\\n",r0,r14,totalcycles;g}',0x2415ac+shift))
machine.debugger:command('temp7=0')
machine.debugger:command('temp8=0')
machine.debugger:command(string.format('bpset %x,temp7<32,{temp7=temp7+1;logerror "nsk1_task_activate: task=%%02x caller=%%08x cycles=%%016x observer_writes=0\\n",r0,r14,totalcycles;g}',0x254cd2+shift))
machine.debugger:command(string.format('bpset %x,temp8<8,{temp8=temp8+1;logerror "nsk1_application_gate: predicate=%%02x boot_reason=%%02x class=%%02x flags=%%02x stage=%%02x startup=%%02x cycles=%%016x observer_writes=0\\n",r0,b@r6,b@r4,b@(r4+1),b@r5,b@%x,totalcycles;g}',0x289218+shift,space:read_u32(0x2894b4+shift)))
machine.debugger:command('temp9=0')
machine.debugger:command(string.format('bpset %x,temp9<8,{temp9=temp9+1;logerror "nsk1_security_checksum: computed=%%04x stored=%%04x cycles=%%016x observer_writes=0\\n",r1,r0,totalcycles;g}',0x22c3dc+shift))
machine.debugger:command('temp1=0')
machine.debugger:command('temp2=0')
machine.debugger:command(string.format('bpset %x,temp1<12,{temp1=temp1+1;logerror "nsk1_report14_gate: remaining=%%02x cycles=%%016x observer_writes=0\\n",r0,totalcycles;g}',0x22a146+shift))
machine.debugger:command(string.format('bpset %x,temp2<12,{temp2=temp2+1;logerror "nsk1_power_window: sample=%%04x lower=%%04x cycles=%%016x observer_writes=0\\n",r1,r2,totalcycles;g}',0x26281c+shift))
local fields = {}
for _,field in pairs(machine.ioport.ports[':COL.1'].fields) do fields[field.mask] = field end
local events = {{12,4,1},{12.2,4,0},{14,16,1},{14.2,16,0},{16,2,1},{16.2,2,0}}
local next_event,reads,writes = 1,0,0
local handles = {}
local power_writes = 0
local power_previous = {}
handles[#handles+1] = space:install_write_tap(0x10b300,0x10b307,'nsk1_power_state',function(offset,data,mask)
    local field,value
    if offset == 0x10b300 and (mask & 0xffff) ~= 0 then field,value = 'event',data & 0xffff end
    if offset == 0x10b304 and (mask & 0xffff0000) ~= 0 then field,value = 'state',(data >> 16) & 0xffff end
    if not field or power_writes >= 128 or power_previous[field] == value then return end
    power_previous[field] = value
    power_writes = power_writes+1
    machine:logerror(string.format('nsk1_power_write: field=%s value=%04x pc=%08x t=%.9f observer_writes=0\n',
        field,value,cpu.state['R15'].value,machine.time:as_double()))
end)
local function simi(offset,data,mask,write)
    for lane = 0,3 do
        local address,shift = offset+lane,(3-lane)*8
        if address >= 0x20036 and address <= 0x2003f and (mask & (255 << shift)) ~= 0 then
            if write then writes = writes+1 else reads = reads+1 end
            if (write and writes or reads) <= 16 then
                machine:logerror(string.format('nsk1_simi: direction=%s address=%08x data=%02x pc=%08x t=%.9f observer_writes=0\n',
                    write and 'W' or 'R',address,(data >> shift) & 255,cpu.state['R15'].value,machine.time:as_double()))
            end
        end
    end
end
handles[#handles+1] = space:install_read_tap(0x20034,0x2003f,'nsk1_simi_read',function(offset,data,mask) simi(offset,data,mask,false) end)
handles[#handles+1] = space:install_write_tap(0x20034,0x2003f,'nsk1_simi_write',function(offset,data,mask) simi(offset,data,mask,true) end)
_G.nsk1_input_handles = handles
local flag_writes = 0
local selftest_writes = 0
handles[#handles+1] = space:install_write_tap(0x10fde4,0x10fde7,'nsk1_activation_status',function(offset,data,mask)
    if (mask & 0x00ff0000) == 0 or selftest_writes >= 32 then return end
    selftest_writes = selftest_writes+1
    machine:logerror(string.format('nsk1_activation_status: value=%02x pc=%08x caller=%08x t=%.9f observer_writes=0\n',
        (data >> 16) & 255,cpu.state['R15'].value,cpu.state['R14'].value,machine.time:as_double()))
end)
handles[#handles+1] = space:install_write_tap(ready_flags & ~3,(ready_flags+10) | 3,'nsk1_ready_flags',function(offset,data,mask)
    for lane = 0,3 do
        local address,bit = offset+lane,(3-lane)*8
        if address >= ready_flags and address < ready_flags+11 and (mask & (255 << bit)) ~= 0 then
            flag_writes = flag_writes+1
            if flag_writes <= 40 then
                machine:logerror(string.format('nsk1_ready_flag_write: index=%d value=%02x pc=%08x t=%.9f observer_writes=0\n',
                    address-ready_flags,(data >> bit) & 255,cpu.state['R15'].value,machine.time:as_double()))
            end
        end
    end
end)
local root_writes = 0
handles[#handles+1] = space:install_write_tap(0x10ffb0,0x10ffb3,'nsk1_root_state',function(offset,data,mask)
    if (mask & 0xff000000) == 0 or root_writes >= 16 then return end
    root_writes = root_writes+1
    machine:logerror(string.format('nsk1_root_state: value=%02x pc=%08x caller=%08x t=%.9f observer_writes=0\n',
        (data >> 24) & 255,cpu.state['R15'].value,cpu.state['R14'].value,machine.time:as_double()))
end)
local control_replies = 0
local receiver = 0x22c5f8+shift
handles[#handles+1] = space:install_read_tap(receiver & ~3,(receiver & ~3)+3,'nsk1_control_reply',function()
    if cpu.state['R15'].value ~= receiver or control_replies >= 4 then return end
    local pointer = cpu.state['R0'].value
    if pointer < 0x100000 or pointer+12 > 0x110000 or space:read_u8(pointer+8) ~= 0x0d then return end
    control_replies = control_replies+1
    machine:logerror(string.format('nsk1_control_reply: payload=%02x%02x envelope=%08x/%08x t=%.9f observer_writes=0\n',
        space:read_u8(pointer+8),space:read_u8(pointer+9),space:read_u32(pointer),space:read_u32(pointer+4),machine.time:as_double()))
end)
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if next_event <= #events and now >= events[next_event][1] then
        local event = events[next_event]
        assert(fields[event[2]],'physical NSK-1 contact missing')
        fields[event[2]]:set_value(event[3])
        local flags = {}
        machine:logerror(string.format('nsk1_power_snapshot: state=%04x event=%04x sample=%04x t=%.9f observer_writes=0\n',
            space:read_u16(0x10b304),space:read_u16(0x10b302),space:read_u16(0x10fe58),now))
        for index = 0,10 do flags[#flags+1] = string.format('%02x',space:read_u8(ready_flags+index)) end
        machine:logerror(string.format('nsk1_ready_flags_snapshot: flags=%s t=%.9f observer_writes=0\n',table.concat(flags),now))
        machine:logerror(string.format('nsk1_key_contact: mask=%02x down=%d irq=%02x irqmask=%02x ctrl=%02x column_mask=%02x root_byte=%02x checklist=%02x layout=%02x simi_reads=%d simi_writes=%d t=%.9f physical_only=1\n',
            event[2],event[3],space:read_u8(0x20009),space:read_u8(0x2000b),space:read_u8(0x2000c),space:read_u8(0x20033),space:read_u8(0x10ffb0),space:read_u8(checklist),space:read_u8(layout),reads,writes,now))
        next_event = next_event+1
    end
end,'NSK-1 physical Menu Up C contacts')
