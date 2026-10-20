-- Save/load an offline populated editor, then continue physical Save input.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/\\])'))
dofile(directory .. 'nsb1_sms_draft_input.lua')
local machine = manager.machine
local capture_arm = dofile(directory .. 'arm_architecture_snapshot.lua')
local saved, complete
local retained = {}
local function capture()
    -- Own research configuration backs 64 KiB; do not hash open-bus aliases.
    local arm = capture_arm(machine, 0x10fffc)
    local dsp = assert(machine.devices[':dsp_c54x:cpu'])
    local registers = {}
    for _, name in ipairs({'PC','XPC','A','B','T','TRN','RTN','RC','SP','ST0','ST1','PMST',
                            'AR0','AR1','AR2','AR3','AR4','AR5','AR6','AR7','BRC','RSA','REA',
                            'BK','IFR','IMR','SWWSR','BSCR','SWCR','TIM','PRD','TCR','IDLE','ILLEGAL'}) do
        registers[#registers+1] = string.format('%s:%x',name,assert(dsp.state[name]).value)
    end
    arm.dsp = table.concat(registers, ',')
    return arm
end
retained[#retained+1] = emu.add_machine_pre_save_notifier(function()
    saved = capture()
    machine:logerror(string.format('nsb1_sms_state: event=saved t=%.9f pc=%08x ram=%08x dsp=%s\n',
        saved.time,saved.pc,saved.ram,saved.dsp))
end)
retained[#retained+1] = emu.add_machine_post_load_notifier(function()
    local restored = capture()
    assert(saved, 'missing editor save observation')
    for _, name in ipairs({'time','pc','sp','ram','cpu','dsp'}) do
        assert(saved[name] == restored[name], 'native editor state mismatch: ' .. name)
    end
    complete = true
    machine:logerror(string.format('nsb1_sms_state: event=restored result=pass t=%.9f pc=%08x ram=%08x dsp=%s\n',
        restored.time,restored.pc,restored.ram,restored.dsp))
end)
local runner = coroutine.create(function()
    assert(emu.wait(41.5))
    machine:save('nsb1_sms_editor')
    assert(emu.wait(0.25))
    assert(saved, 'editor save did not execute')
    machine:load('nsb1_sms_editor')
end)
retained[#retained+1] = runner
retained[#retained+1] = emu.add_machine_stop_notifier(function()
    if not complete then machine:logerror('nsb1_sms_state: FAIL incomplete\n') end
end)
_G.nsb1_sms_state_retained = retained
assert(coroutine.resume(runner))
