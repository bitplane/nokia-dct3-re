-- Explicit post-wake status command through the serial peer, not MCU policy.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'mu4_resident_int2_observe.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':cpu'])
local field=assert(machine.ioport.ports[':MCU_CONTROL'].fields['MCU status request'])
local runner=coroutine.create(function()
    assert(emu.wait(11.2))
    local wake=_G.mu4_int2_fixture_assert~=false
    assert(cpu.state['ILLEGAL'].value==0 and cpu.state['IMR'].value==(wake and 0x0ac5 or 4) and
        cpu.state['GENPC'].value==(wake and 0x298ea or 0x28923),'status query context differs from declared wake/control')
    field:set_value(1)
    assert(emu.wait(0.1))
    field:clear_value()
    machine:logerror('mu4_resident_status_fixture: complete=1 token=04 source=bench_serial_command normal_mcu_policy=unproved firmware_state_forcing=0\n')
end)
_G.mu4_resident_status_runner=runner
assert(coroutine.resume(runner))
