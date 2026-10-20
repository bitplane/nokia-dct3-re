-- Read-only NSK-1 startup instrument. No keys, RAM writes or peer replies.
-- Keep only accepted-copy/restart diagnostics; complete record exchanges are
-- captured below independently of the debugger.
local machine = manager.machine
assert(machine.system.name == 'nsk1r4t')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local dsp = machine.devices[':dsp_c54x:cpu']
local shift = space:read_u32(0x296d08) == 112 and 0 or -0x10
local reason_address = space:read_u32(0x28937c+shift)
local idle_address = space:read_u32(0x2894d4+shift)
assert(reason_address >= 0x100000 and reason_address < 0x110000)
assert(idle_address >= 0x100000 and idle_address < 0x110000)
if machine.debugger then
    machine.debugger:command('focus :maincpu')
    machine.debugger:command('temp0=0')
    machine.debugger:command(string.format('bpset %x,temp0<80,{temp0=temp0+1;logerror "nsk1_startup_entry: pc=%%08x r0=%%08x caller=%%08x cycles=%%016x observer_writes=0\\n",pc,r0,r14,totalcycles;g}', 0x295136+shift))
    machine.debugger:command(string.format('bpset %x,temp0<80,{temp0=temp0+1;logerror "nsk1_record_decision: pointer=%%08x state=%%08x valid=%%08x observer_writes=0\\n",r6,r12,r5;g}', 0x25a994+shift))
    machine.debugger:command(string.format('bpset %x,temp0<80,{temp0=temp0+1;logerror "nsk1_record_accept: context=%%08x valid=%%08x copied=%%08x%%08x%%08x%%08x%%08x%%08x observer_writes=0\\n",r4,r5,d@(r4+1c),d@(r4+20),d@(r4+24),d@(r4+28),d@(r4+2c),d@(r4+30);g}', 0x25ab0c+shift))
end
local serial_selection, lcd_commands, lcd_bytes, lcd_nonzero = 0, 0, 0, 0
_G.nsk1_lcd_tap = space:install_write_tap(0x20028, 0x2002f, 'nsk1_lcd', function(offset, data, mask)
    if offset == 0x20028 and (mask & 0xff000000) ~= 0 then serial_selection = (data >> 24) & 255 end
    if serial_selection ~= 0x21 then return end
    if offset == 0x2002c and (mask & 0xff000000) ~= 0 then
        lcd_commands = lcd_commands+1
        if lcd_commands <= 32 then
            machine:logerror(string.format('nsk1_lcd_command: byte=%02x pc=%08x t=%.9f observer_writes=0\n',
                (data >> 24) & 255, cpu.state['R15'].value, machine.time:as_double()))
        end
    elseif offset == 0x20028 and (mask & 255) ~= 0 then
        lcd_bytes = lcd_bytes+1
        if (data & 255) ~= 0 then lcd_nonzero = lcd_nonzero+1 end
    end
end)
local releases, released, shared_writes = 0, false, 0
local record_handles, record_requests, record_replies = {}, 0, 0
local function ram_bytes(pointer, count)
    assert(pointer >= 0x100000 and pointer+count <= 0x110000)
    local bytes = {}
    for index = 0, count-1 do bytes[#bytes+1] = string.format('%02x', space:read_u8(pointer+index)) end
    return table.concat(bytes)
end
local sender = 0x255268+shift
record_handles[#record_handles+1] = space:install_read_tap(sender & ~3, (sender & ~3)+3, 'nsk1_record_request', function()
    if cpu.state['R15'].value ~= sender or cpu.state['R0'].value ~= 3 or record_requests >= 16 then return end
    local pointer = cpu.state['R1'].value
    if pointer < 0x100000 or pointer+32 > 0x110000 or space:read_u8(pointer+3) ~= 0x70 then return end
    local command, length = space:read_u8(pointer+4), space:read_u8(pointer+5)
    if command < 0x13 or command > 0x16 then return end
    assert(length <= 24)
    record_requests = record_requests+1
    machine:logerror(string.format('nsk1_record_request: command=%02x bytes=%s caller=%08x t=%.9f observer_writes=0\n',
        command, ram_bytes(pointer,6+length), cpu.state['R14'].value, machine.time:as_double()))
end)
for _, entry in ipairs({0x25a620,0x25a814}) do
    local address = entry+shift
    record_handles[#record_handles+1] = space:install_read_tap(address & ~3, (address & ~3)+3, 'nsk1_record_reply', function()
        if cpu.state['R15'].value ~= address or record_replies >= 16 then return end
        record_replies = record_replies+1
        local pointer = cpu.state['R0'].value
        local actual_length = space:read_u16(pointer+4)-2
        assert(actual_length >= 4 and actual_length <= 64,'own record reply exceeds bounded capture')
        machine:logerror(string.format('nsk1_record_reply: consumer=%08x envelope=%s payload=%s t=%.9f observer_writes=0\n',
            address, ram_bytes(pointer,8), ram_bytes(pointer+8,actual_length), machine.time:as_double()))
    end)
end
_G.nsk1_record_handles = record_handles
_G.nsk1_release_tap = space:install_write_tap(0x20000, 0x20003, 'nsk1_release', function(offset, data, mask)
    if (mask & 0xff00) == 0 then return end
    local command = (data >> 8) & 255
    if (command & 1) == 0 then released = false; return end
    if released then return end
    released = true
    releases = releases + 1
    if releases > 8 then return end
    local arguments, payload = {}, {}
    for index = 0, 6 do arguments[#arguments+1] = string.format('%04x', space:read_u16(0x100f6+index*2)) end
    for index = 0, 185 do payload[#payload+1] = string.format('%04x', space:read_u16(0x11e00+index*2)) end
    machine:logerror(string.format('nsk1_release: index=%d command=%02x pc=%08x args=%s program=%s t=%.9f observer_writes=0\n',
        releases, command, cpu.state['R15'].value, table.concat(arguments), table.concat(payload), machine.time:as_double()))
end)
_G.nsk1_shared_tap = space:install_write_tap(0x10000, 0x10fff, 'nsk1_shared', function(offset, data, mask)
    shared_writes = shared_writes + 1
end)
local times, next_sample = {0.1, 0.5, 1, 2, 5}, 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if next_sample > #times or now < times[next_sample] then return end
    machine:logerror(string.format('nsk1_boot: t=%.9f pc=%08x sp=%08x dsp_pc=%04x illegal=%d idle=%d result=%04x/%04x/%04x/%04x catalogue25=%08x shared_writes=%d releases=%d observer_writes=0\n',
        now, cpu.state['R15'].value, cpu.state['R13'].value,
        dsp.state['PC'].value, dsp.state['ILLEGAL'].value, dsp.state['IDLE'].value,
        space:read_u16(0x10000), space:read_u16(0x10002), space:read_u16(0x10004), space:read_u16(0x10006),
        space:read_u32(0x10b2c4), shared_writes, releases))
    machine:logerror(string.format('nsk1_presentation: t=%.9f commands=%d data=%d nonzero=%d startup_reason=%02x idle_flag=%02x observer_writes=0\n',
        now, lcd_commands, lcd_bytes, lcd_nonzero, space:read_u8(reason_address), space:read_u8(idle_address)))
    machine.screens[':screen']:snapshot(string.format('nsk1_native_sample%d.png', next_sample))
    if next_sample == #times then machine.screens[':screen']:snapshot('nsk1_native_5s.png') end
    next_sample = next_sample+1
end, 'NSK-1 passive native boundary')
local late_times, late_index = {10,20,30,40,50,59},1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if late_index > #late_times or now < late_times[late_index] then return end
    machine:logerror(string.format('nsk1_late: t=%.9f pc=%08x dsp_pc=%04x illegal=%d startup_reason=%02x idle_flag=%02x commands=%d data=%d nonzero=%d observer_writes=0\n',
        now,cpu.state['R15'].value,dsp.state['PC'].value,dsp.state['ILLEGAL'].value,
        space:read_u8(reason_address),space:read_u8(idle_address),lcd_commands,lcd_bytes,lcd_nonzero))
    machine.screens[':screen']:snapshot(string.format('nsk1_native_%ds.png',late_times[late_index]))
    late_index = late_index+1
end,'NSK-1 late passive boundary')
