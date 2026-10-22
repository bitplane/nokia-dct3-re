-- Save/load only; native serial peer and firmware retain all protocol ownership.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..(_G.mu4_serial_restore_observer or 'mu4_playback_serial_control_observe.lua'))
local machine=manager.machine
local cpu=assert(machine.devices[':cpu'])
local data=assert(cpu.spaces['data'])
local program=assert(machine.memory.shares[':program_ram'])
local saved,anchor,phase,complete
local window,cap=0.15,4096
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
    -- Exclude peripheral/MMR reads. The remainder of this bench's data map
    -- is RAM; the program share is read directly, bypassing execution taps.
    for address=0x80,0xffff do ram=((ram<<5)-ram+data:read_u16(address)) & 0xffffffff end
    assert(program.size==0x1000000,'program backing size changed')
    for address=0,program.size-4,4 do
        backing=((backing<<5)-backing+program:read_u32(address)) & 0xffffffff
    end
    return string.format('t=%.9f cpu=%s ram=%08x program=%08x data_words=65408 program_bytes=16777216 observer_writes=0',
        machine.time:as_double(),table.concat(registers,','),ram,backing)
end
retained[#retained+1]=data:install_write_tap(0x33,0x33,'mu4_control_replay_tx',function(address,value,mask)
    if not anchor or machine:side_effects_disabled() then return end
    local now=machine.time:as_double()
    if now<anchor or now>=anchor+window then return end
    local branch=assert(events[phase])
    assert(#branch<cap,'serial replay observation capped')
    local event=string.format('t=%.9f address=%04x value=%04x mask=%04x',now,address,value,mask)
    machine:logerror(string.format('mu4_control_replay_event: phase=%s index=%d %s observer_writes=0\n',phase,#branch,event))
    branch[#branch+1]=event
end)
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    assert(not saved,'unexpected second checkpoint')
    saved=snapshot()
    anchor,phase=machine.time:as_double(),'reference'
    machine:logerror('mu4_control_state: phase=saved '..saved..'\n')
end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=snapshot()
    machine:logerror('mu4_control_state: phase=restored '..restored..'\n')
    assert(saved and restored==saved,'native control CPU/RAM checksum snapshot differs')
    phase='restored'
end)
retained[#retained+1]=emu.register_frame_done(function()
    if phase~='restored' or complete or machine.time:as_double()<anchor+window then return end
    assert(#events.reference>0 and #events.reference==#events.restored,'serial replay event count differs')
    for index,event in ipairs(events.reference) do
        assert(events.restored[index]==event,'serial replay ordering/value/time differs at '..index)
    end
    complete=true
    machine:logerror(string.format('mu4_control_replay: result=pass reference_events=%d restored_events=%d window=%.9f cap=%d observer_writes=0 audio_acceptance=0\n',
        #events.reference,#events.restored,window,cap))
end)
local runner=coroutine.create(function()
    assert(emu.wait(_G.mu4_serial_restore_time or 1.05))
    machine:save('mu4_control_pending')
    assert(emu.wait(0.25))
    assert(saved,'native control checkpoint did not execute')
    machine:load('mu4_control_pending')
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    machine:logerror('mu4_control_restore_terminal: complete='..(complete and '1' or '0')..' observer_writes=0 audio_acceptance=0\n')
end)
_G.mu4_control_restore_retained=retained
assert(coroutine.resume(runner))
