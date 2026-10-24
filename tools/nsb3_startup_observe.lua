-- Bounded, read-only startup probes. Debugger counters are not handset state.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
for index, address in ipairs({0x2946b0, 0x294ad8, 0x291126, 0x28e016}) do
    local counter = 'temp' .. (index - 1)
    machine.debugger:command(counter .. '=0')
    cpu.debug:bpset(address, counter .. '<10',
        string.format('%s=%s+1;logerror "nsb3_startup: site=%%08x r0=%%08x r1=%%08x r6=%%08x selector=%%04x lr=%%08x observer_writes=0\\n",pc,r0,r1,r6,w@10ad0a,r14;g', counter, counter))
end
local samples = coroutine.create(function()
    assert(emu.wait(5))
    machine:logerror(string.format(
        'nsb3_idle_state: latch=%02x companion=%02x ctrl=%02x fiq=%02x irq=%02x fmask=%02x imask=%02x selector=%04x program0=%04x\n',
        memory:read_u8(0x10c4c0), memory:read_u8(0x10c304), memory:read_u8(0x2000c),
        memory:read_u8(0x20008), memory:read_u8(0x20009), memory:read_u8(0x2000a),
        memory:read_u8(0x2000b), memory:read_u16(0x10ad0a), memory:read_u16(0x10e00)))
    local stream = assert(io.open('nsb3_uploaded_program.bin', 'wb'))
    for offset = 0, 185 do stream:write(string.pack('>I2', memory:read_u16(0x10e00 + offset * 2))) end
    stream:close()
    local dsp = machine.devices[':dsp_c54x:cpu']
    if dsp then
        machine:logerror(string.format('nsb3_native_state: pc=%04x illegal=%d idle=%d observer_writes=0\n',
            dsp.state['PC'].value, dsp.state['ILLEGAL'].value, dsp.state['IDLE'].value))
    end
    machine.screens[':screen']:snapshot('nsb3_startup_5s.png')
end)
assert(coroutine.resume(samples))
