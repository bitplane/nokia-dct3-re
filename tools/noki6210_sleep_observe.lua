-- Bounded observation of the ordinary outgoing-call fixture; no MMIO/RAM writes.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
machine.debugger:command('temp4=0')
machine.debugger:command('temp5=0')
-- Expiry receives the record pointer, not timer ID c7; do not compare r0 with c7.
cpu.debug:bpset(0x3c1bb4, 'r0==100b10 && temp5<128',
    'temp5=temp5+1;logerror "6210_sleep_timer_expire: record=%08x interval=%04x owner=%02x flags=%02x state=%02x id=%02x ctrl=%02x lr=%08x observer_writes=0\\n",r0,w@100b14,b@100b16,b@100b17,b@100b18,b@100b19,b@2000c,r14;g')
cpu.debug:bpset(0x3c19ba, 'r0==c7 && temp4<1024',
    'temp4=temp4+1;logerror "6210_sleep_timer_arm: id=%02x delay=%04x lr=%08x owner=%02x flags=%02x state=%02x stored_id=%02x observer_writes=0\\n",r0,r1,r14,b@100b16,b@100b17,b@100b18,b@100b19;g')
local transitions, previous = {}, nil
_G.npe3_sleep_control_tap = memory:install_write_tap(0x2000c, 0x2000f,
    'npe3_sleep_control', function(address, data, mask)
        if address ~= 0x2000c or mask & 0xff000000 == 0 then return end
        local control = (data >> 24) & 0xff
        if control == previous then return end
        previous = control
        transitions[#transitions + 1] = string.format(
            '6210_sleep_control: value=%02x pc=%x lr=%x t=%.9f\n',
            control, cpu.state.R15.value, cpu.state.R14.value, machine.time:as_double())
        if #transitions > 16 then table.remove(transitions, 1) end
    end)
local runner = coroutine.create(function()
    assert(emu.wait(31.5))
    machine.debugger:command('temp6=0')
    cpu.debug:bpset(0x3c1c08, 'temp6<16',
        'temp6=temp6+1;logerror "6210_sleep_timer_service: head=%08x interval=%04x ctrl=%02x lr=%08x observer_writes=0\\n",d@100048,w@100b14,b@2000c,r14;g')
    machine.debugger:command('temp7=0')
    cpu.debug:bpset(0x4cc3b2, 'r0==100b14 && temp7<16',
        'temp7=temp7+1;logerror "6210_sleep_timer_adjust: interval=%04x timer0=%04x baseline=%04x ctrl=%02x lr=%08x observer_writes=0\\n",w@100b14,w@20010,w@173fe8,b@2000c,r14;g')
    -- Install after the first wake; debugger counters are not handset state.
    local checkpoints = {0x500a18, 0x4dc12c, 0x4b03ec, 0x4b0412}
    for index, address in ipairs(checkpoints) do
        local counter = 'temp' .. (index - 1)
        machine.debugger:command(counter .. '=0')
        cpu.debug:bpset(address, counter .. '<8',
            string.format('%s=%s+1;logerror "6210_sleep_continuation: pc=%%08x r0=%%08x lr=%%08x ctrl=%%02x dsp=%%02x observer_writes=0\\n",pc,r0,r14,b@2000c,b@20002;g',
                          counter, counter))
    end
    assert(emu.wait(35.9 - machine.time:as_double()))
    -- Earlier caps can saturate before the deadline/key edge; sample this window separately.
    machine.debugger:command('temp8=0')
    cpu.debug:bpset(0x4b040e, 'temp8<16',
        'temp8=temp8+1;logerror "6210_sleep_late_permission: result=%08x ctrl=%02x interval=%04x observer_writes=0\\n",r0,b@2000c,w@100b14;g')
    machine.debugger:command('temp9=0')
    cpu.debug:bpset(0x500a18, 'temp9<16',
        'temp9=temp9+1;logerror "6210_sleep_late_request: argument=%08x ctrl=%02x interval=%04x observer_writes=0\\n",r0,b@2000c,w@100b14;g')
    -- The predicate adjusts a stack copy, not the queue's stored interval.
    machine.debugger:command('temp6=0')
    cpu.debug:bpset(0x4cc3e2, 'temp6<16',
        'temp6=temp6+1;logerror "6210_sleep_deadline_entry: flags=%02x stored=%04x timer0=%04x baseline=%04x timer1=%04x destination=%04x fiq=%02x ctrl=%02x observer_writes=0\\n",b@100024,w@100b14,w@20010,w@173fe8,w@20004,w@20006,b@20008,b@2000c;g')
    machine.debugger:command('temp7=0')
    cpu.debug:bpset(0x4cc43a, 'temp7<16',
        'temp7=temp7+1;logerror "6210_sleep_deadline_reject: r0=%08x r1=%08x r2=%08x r3=%08x observer_writes=0\\n",r0,r1,r2,r3;g')
    for _, target in ipairs({35.99, 36.05, 36.2, 36.4}) do
        assert(emu.wait(target - machine.time:as_double()))
        if target == 35.99 then
            for _, line in ipairs(transitions) do machine:logerror(line) end
        end
        local head = memory:read_u32(0x100048)
        local interval = -1
        if head >= 0x100000 and head < 0x180000 then
            interval = memory:read_u16(head + 4)
        end
        machine:logerror(string.format(
            '6210_sleep_state: port=%02x fiq=%02x irq=%02x fmask=%02x imask=%02x ctrl=%02x timer0=%04x baseline=%04x timer1=%04x destination=%04x head=%08x interval=%d cpsr=%08x idle_veto=%02x timer_owner=%02x timer_flags=%02x timer_state=%02x timer_id=%02x t=%.9f\n',
            machine.ioport.ports[':COL.0']:read(), memory:read_u8(0x20008),
            memory:read_u8(0x20009), memory:read_u8(0x2000a), memory:read_u8(0x2000b),
            memory:read_u8(0x2000c), memory:read_u16(0x20010), memory:read_u16(0x173fe8),
            memory:read_u16(0x20004), memory:read_u16(0x20006), head, interval,
            cpu.state.CPSR.value, memory:read_u8(0x17556c), memory:read_u8(0x100b16),
            memory:read_u8(0x100b17), memory:read_u8(0x100b18), memory:read_u8(0x100b19),
            machine.time:as_double()))
    end
end)
_G.npe3_sleep_observer = runner
assert(coroutine.resume(runner))
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/])') .. 'noki6210_outgoing_call_input.lua')
