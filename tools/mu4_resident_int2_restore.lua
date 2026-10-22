-- Save/load plus a repeated declared board-pin pulse; no firmware mutation.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'mu4_playback_serial_control_observe.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':cpu'])
local data=assert(cpu.spaces['data'])
local program=assert(machine.memory.shares[':program_ram'])
local field=assert(machine.ioport.ports[':MCU_CONTROL'].fields['MCU DSP interrupt asserted'])
local phase,anchor,saved,complete,stage
local cap,window=2048,0.25
local events={reference={},restored={}}
local retained={}
local function snapshot()
    local registers={}
    for _,name in ipairs({'PC','XPC','A','B','T','TRN','RTN','RC','SP','ST0','ST1','PMST',
        'AR0','AR1','AR2','AR3','AR4','AR5','AR6','AR7','BRC','RSA','REA','BK','IFR','IMR',
        'SWWSR','BSCR','SWCR','TIM','PRD','TCR','IDLE','ILLEGAL','GENPC','CURPC'}) do
        registers[#registers+1]=string.format('%s:%010x',name,cpu.state[name].value & 0xffffffffff)
    end
    local ram,backing=0,0
    for address=0x80,0xffff do ram=((ram<<5)-ram+data:read_u16(address)) & 0xffffffff end
    assert(program.size==0x1000000,'program backing size changed')
    for address=0,program.size-4,4 do backing=((backing<<5)-backing+program:read_u32(address)) & 0xffffffff end
    return string.format('t=%.9f cpu=%s ram=%08x program=%08x data_words=65408 program_bytes=16777216 observer_writes=0',
        machine.time:as_double(),table.concat(registers,','),ram,backing)
end
retained[#retained+1]=data:install_write_tap(0,1,'mu4_wake_replay_registers',function(address,value,mask)
    if not phase or complete or machine:side_effects_disabled() then return end
    local now=machine.time:as_double()
    if now<anchor or now>=anchor+window then return end
    local branch=events[phase]
    assert(#branch<cap,'wake replay observation capped')
    local event=string.format('t=%.9f address=%04x value=%04x mask=%04x',now,address,value,mask)
    machine:logerror(string.format('mu4_wake_replay_event: phase=%s index=%d %s observer_writes=0\n',phase,#branch,event))
    branch[#branch+1]=event
end)
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    assert(not saved,'unexpected second wake checkpoint')
    assert(cpu.state['IDLE'].value==1 and cpu.state['IMR'].value==4 and cpu.state['GENPC'].value==0x28923,
        'wake checkpoint must precede pin assertion in resident deep idle')
    saved=snapshot()
    anchor,phase,stage=machine.time:as_double(),'reference',0
    machine:logerror('mu4_wake_restore_state: phase=saved '..saved..'\n')
end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=snapshot()
    machine:logerror('mu4_wake_restore_state: phase=restored '..restored..'\n')
    assert(saved and restored==saved,'wake checkpoint CPU/RAM state differs')
    phase,stage='restored',0
end)
retained[#retained+1]=emu.register_frame_done(function()
    if not phase or complete then return end
    local now=machine.time:as_double()
    if stage==0 and now>=anchor+0.05 then
        field:set_value(1)
        stage=1
        machine:logerror(string.format('mu4_wake_replay_input: phase=%s asserted=1 t=%.9f source=bench_pin firmware_state_forcing=0\n',phase,now))
    elseif stage==1 and now>=anchor+0.10 then
        field:clear_value()
        stage=2
        machine:logerror(string.format('mu4_wake_replay_input: phase=%s asserted=0 t=%.9f source=bench_pin firmware_state_forcing=0\n',phase,now))
    end
    if stage~=2 or now<anchor+window then return end
    assert(cpu.state['ILLEGAL'].value==0 and cpu.state['IMR'].value==0x0ac5 and
        cpu.state['IDLE'].value==1 and cpu.state['GENPC'].value==0x298ea,'wake replay did not reach native worker idle')
    if phase=='reference' then
        stage=3
        machine:load('mu4_resident_before_int2')
    else
        assert(#events.reference>0 and #events.reference==#events.restored,'wake replay event count differs')
        for index,event in ipairs(events.reference) do
            assert(events.restored[index]==event,'wake replay value/time differs at '..index)
        end
        complete=true
        machine:logerror(string.format('mu4_wake_replay: result=pass reference_events=%d restored_events=%d window=%.9f cap=%d observer_writes=0 normal_mcu_waveform=unproved audio_acceptance=0\n',
            #events.reference,#events.restored,window,cap))
    end
end)
local runner=coroutine.create(function()
    assert(emu.wait(10.95))
    machine:save('mu4_resident_before_int2')
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    machine:logerror('mu4_wake_restore_terminal: complete='..(complete and '1' or '0')..' observer_writes=0 audio_acceptance=0\n')
end)
_G.mu4_wake_restore_retained=retained
assert(coroutine.resume(runner))
