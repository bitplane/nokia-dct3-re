-- Restore active native output without touching guest state or physical input.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
local now,waits=0,{}
local environment={manager={machine={time={as_double=function() return now end}}},
    emu={wait=function(delay)
        waits[#waits+1]=delay
        if #waits==1 then now=.5; return false end
        now=1; return true
    end}}
local deadline=assert(loadfile(directory..'fixture_deadline.lua','t',environment))()
deadline(1)
deadline(1)
assert(#waits==2 and waits[1]==1 and waits[2]==.5 and now==1)
dofile(directory..'nsm1_factory_tone.lua')
local machine=manager.machine
machine:logerror('nsm1_deadline_conformance: false_wakeup=1 waits=2 completed=1 observer_writes=0\n')
local dsp=machine.devices[':dsp_c54x:cpu']
local arm=machine.devices[':maincpu']
local stage,saved,restored=0,nil,false
local function snapshot()
    local fields={string.format('t=%.9f',machine.time:as_double())}
    for _,name in ipairs({'PC','SP','ST0','ST1','A','B','T','AR0','AR1','AR2','AR3','AR4','AR5','AR6','AR7','IFR','IMR'}) do
        fields[#fields+1]=string.format('dsp_%s=%010x',name,dsp.state[name].value&0xffffffffff)
    end
    assert(arm.state.PC.value==arm.state.GENPC.value,'ARM PC export differs')
    fields[#fields+1]=string.format('mcu_PC=%08x',arm.state.PC.value)
    for i=0,15 do fields[#fields+1]=string.format('mcu_R%d=%08x',i,arm.state['R'..i].value) end
    fields[#fields+1]=string.format('mcu_CPSR=%08x',arm.state.CPSR.value)
    -- BDRR is destructive and must not be sampled by an observer.
    for _,address in ipairs({0x21,0x22,0x23,0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe}) do
        fields[#fields+1]=string.format('data_%04x=%04x',address,dsp.spaces.data:read_u16(address))
    end
    return table.concat(fields,' ')
end
local before=emu.add_machine_pre_save_notifier(function()
    assert(stage==1 and not saved)
    saved=snapshot()
    machine:logerror('nsm1_pcm_restore: phase=saved '..saved..'\n')
end)
local after=emu.add_machine_post_load_notifier(function()
    local current=snapshot()
    machine:logerror('nsm1_pcm_restore: phase=restored '..current..'\n')
    assert(stage==2 and current==saved,'active native tone snapshot differs')
    restored=true
end)
local frames=emu.register_frame_done(function()
    local t=machine.time:as_double()
    if stage==0 and t>=86.2 then
        stage=1; machine:save('nsm1_active_tone')
    elseif stage==1 and saved and t>=86.25 then
        stage=2; machine:load('nsm1_active_tone')
    elseif stage==2 and restored and t>=86.25 then
        stage=3
        machine:logerror(string.format('nsm1_pcm_restore: phase=complete t=%.9f physical_release_replayed=0\n',t))
    end
end)
local stop=emu.add_machine_stop_notifier(function() assert(stage==3,'native tone replay incomplete') end)
_G.nsm1_pcm_restore={before,after,frames,stop}
