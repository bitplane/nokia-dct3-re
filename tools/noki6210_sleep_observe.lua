-- Bounded observation of the ordinary outgoing-call fixture; no MMIO/RAM writes.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
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
    -- Install after the first wake; debugger counters are not handset state.
    local checkpoints = {0x500a18, 0x4dc12c, 0x4b03ec, 0x4b0412}
    for index, address in ipairs(checkpoints) do
        local counter = 'temp' .. (index - 1)
        machine.debugger:command(counter .. '=0')
        cpu.debug:bpset(address, counter .. '<8',
            string.format('%s=%s+1;logerror "6210_sleep_continuation: pc=%%08x r0=%%08x lr=%%08x ctrl=%%02x dsp=%%02x observer_writes=0\\n",pc,r0,r14,b@2000c,b@20002;g',
                          counter, counter))
    end
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
            '6210_sleep_state: port=%02x fiq=%02x irq=%02x fmask=%02x imask=%02x ctrl=%02x timer0=%04x baseline=%04x timer1=%04x destination=%04x head=%08x interval=%d cpsr=%08x idle_veto=%02x t=%.9f\n',
            machine.ioport.ports[':COL.0']:read(), memory:read_u8(0x20008),
            memory:read_u8(0x20009), memory:read_u8(0x2000a), memory:read_u8(0x2000b),
            memory:read_u8(0x2000c), memory:read_u16(0x20010), memory:read_u16(0x173fe8),
            memory:read_u16(0x20004), memory:read_u16(0x20006), head, interval,
            cpu.state.CPSR.value, memory:read_u8(0x17556c), machine.time:as_double()))
    end
end)
_G.npe3_sleep_observer = runner
assert(coroutine.resume(runner))
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/])') .. 'noki6210_outgoing_call_input.lua')
