-- Own flag/checksum observations; no firmware or storage writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_acquisition_observe.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local count = 0
_G.nsb3_selftest_tap = memory:install_write_tap(0x10fde8, 0x10fdeb,
    'nsb3_selftest_flags', function(address, data, mask)
        if mask & 0x00ff0000 == 0 then return end
        count = count + 1
        if count <= 32 then
            machine:logerror(string.format('nsb3_selftest_flag: value=%02x pc=%08x t=%.9f observer_writes=0\n',
                (data >> 16) & 255, cpu.state.R15.value, machine.time:as_double()))
        end
    end)
cpu.debug:bpset(0x23d0c0, '1',
    'logerror "nsb3_checksum_compare: stored=%04x computed=%04x secondary=%04x observer_writes=0\\n",r0,r1,w@(r13+6);g')
cpu.debug:bpset(0x23d144, '1',
    'logerror "nsb3_selftest_error: index=%02x code=%02x pointer=%08x observer_writes=0\\n",r0,b@r2,r2;g')
local sample = coroutine.create(function()
    assert(emu.wait(5))
    machine:logerror(string.format('nsb3_selftest_final: flags=%02x remaining=%02x observer_writes=0\n',
        memory:read_u8(0x10fde9), memory:read_u8(0x10a0e2)))
end)
assert(coroutine.resume(sample))
