-- Let the ordinary Timer1/FIQ5 event complete PLAY TONE; no key or IRQ injection.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_tone_hold.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
for _,pc in ipairs({0x275106,0x2bd8de}) do
    local count=0
    taps[#taps+1]=memory:install_read_tap(pc&~3,(pc&~3)+3,'nsm1_tone_timing_'..pc,function()
        local t=machine.time:as_double()
        if t<85 or t>=93.5 or cpu.state['PC'].value~=pc then return end
        if cpu.state['R0'].value~=(pc==0x275106 and 0x52 or 0x4190) then return end
        count=count+1
        assert(count<16,'tone timing entry cap')
        machine:logerror(string.format('nsm1_tone_timing_entry: pc=%08x r0=%08x r1=%08x caller=%08x t=%.9f observer_writes=0\n',pc,cpu.state['R0'].value,cpu.state['R1'].value,cpu.state['R14'].value,t))
    end)
end
local writes=0
taps[#taps+1]=memory:install_write_tap(0x100510,0x10051b,'nsm1_tone_timing_record',function(address,value,mask)
    local t=machine.time:as_double()
    if t<85 or t>=93.5 then return end
    writes=writes+1
    assert(writes<64,'tone timing record cap')
    machine:logerror(string.format('nsm1_tone_timing_write: address=%08x value=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',address,value,mask,cpu.state['PC'].value,t))
end)
taps[#taps+1]=memory:install_write_tap(0x20008,0x2000b,'nsm1_tone_timing_fiq5',function(_,value,mask)
    local t=machine.time:as_double()
    if t<85 or t>=93.5 or mask&0xff000000==0 or (value>>24)&0x20==0 then return end
    machine:logerror(string.format('nsm1_tone_timing_fiq5_ack: data=%02x pc=%08x t=%.9f observer_writes=0\n',(value>>24)&255,cpu.state['PC'].value,t))
end)
_G.nsm1_tone_timing_taps=taps
local actions=coroutine.create(function()
    assert(emu.wait(93.3))
    local dsp=machine.devices[':dsp_c54x:cpu']
    machine:logerror(string.format('nsm1_tone_timing_final: record=%08x%08x%08x pc=%04x idle=%d stop=%d t=%.9f observer_writes=0\n',memory:read_u32(0x100510),memory:read_u32(0x100514),memory:read_u32(0x100518),dsp.state['PC'].value,dsp.state['IDLE'].value,dsp.state['ILLEGAL'].value,machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsm1_toolkit_tone_late.png')
    assert(emu.wait(.2))
    machine:logerror('nsm1_tone_timing_exit: t=93.500000000 observer_writes=0\n')
    machine:exit()
end)
assert(coroutine.resume(actions))
