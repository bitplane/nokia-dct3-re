-- Declared logical pulse on schematic net UC_DSP_INT; no MSP430 claim.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'mu4_playback_serial_control_observe.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':cpu'])
local field=assert(machine.ioport.ports[':MCU_CONTROL'].fields['MCU DSP interrupt asserted'])
local pulse=_G.mu4_int2_fixture_assert~=false
local retained={}
machine.debugger:command('temp1=0')
for _,site in ipairs({{0x28914,'enter'},{0x28925,'resume'},{0x28926,'restore_mask'},{0x3ac0d,'caller'}}) do
    -- Debugger integer literals are hexadecimal: 20 is the declared 32 events.
    retained[#retained+1]=cpu.debug:bpset(site[1],'temp1<20 && pw@28922==f5e1',
        'temp1=temp1+1;logerror "mu4_int2_path: ordinal=%d kind='..site[2]..
        ' pc=%06x opcode=%04x imr=%04x ifr=%04x sp=%04x st1=%04x identity=Execute_entry operands=context_only observer_writes=0\\n",temp1,curpc,pw@curpc,imr,ifr,sp,st1;g')
end
local writes,write_cap=0,1024
retained[#retained+1]=cpu.spaces['data']:install_write_tap(0,1,'mu4_int2_register_writes',function(address,value,mask)
    if machine:side_effects_disabled() then return end
    local now=machine.time:as_double()
    if now<11 or now>=11.2 then return end
    if writes>=write_cap then writes=write_cap;return end
    local before=cpu.state[address==0 and 'IMR' or 'IFR'].value
    machine:logerror(string.format('mu4_int2_register_write: index=%d t=%.9f address=%04x before=%04x value=%04x mask=%04x context_pc=%06x identity=data_write operands=context_only observer_writes=0\n',
        writes,now,address,before,value,mask,cpu.state['GENPC'].value))
    writes=writes+1
end)
local function snapshot(phase)
    machine:logerror(string.format('mu4_int2_state: phase=%s t=%.9f pc=%06x idle=%d imr=%04x ifr=%04x st1=%04x illegal=%d observer_writes=0\n',
        phase,machine.time:as_double(),cpu.state['GENPC'].value,cpu.state['IDLE'].value,
        cpu.state['IMR'].value,cpu.state['IFR'].value,cpu.state['ST1'].value,cpu.state['ILLEGAL'].value))
end
local runner=coroutine.create(function()
    assert(emu.wait(11))
    snapshot('before')
    assert(cpu.state['IDLE'].value==1 and cpu.state['IMR'].value==4,'native INT2 fixture must start at resident deep idle')
    if pulse then field:set_value(1) end
    assert(emu.wait(0.05))
    if pulse then field:clear_value() end
    snapshot('released')
    assert(emu.wait(0.15))
    snapshot('after')
    machine:logerror('mu4_int2_fixture: complete=1 pulse_asserted='..(pulse and '1' or '0')..' declared_pulse_ms=50 normal_mcu_waveform=unproved firmware_state_forcing=0\n')
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "mu4_int2_paths_terminal: events=%d cap=32 observer_writes=0\\n",temp1')
    machine:logerror(string.format('mu4_int2_writes_terminal: events=%d cap=%d observer_writes=0\n',writes,write_cap))
end)
_G.mu4_int2_retained=retained
assert(coroutine.resume(runner))
