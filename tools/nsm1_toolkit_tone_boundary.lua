-- Bounded publication census, with early-boot positive controls; no guest writes.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_tone_hold.lua')
local machine=manager.machine
local arm=machine.devices[':maincpu']
local dsp=machine.devices[':dsp_c54x:cpu']
local taps,counts={}, {mcu_early=0,mcu=0,dsp=0,io=0,compiler_early=0,compiler=0}
local function late() local t=machine.time:as_double(); return t>=85 and t<91.5 end
taps[#taps+1]=arm.spaces.program:install_read_tap(0x2a4a6c,0x2a4a6f,'nsm1_tone_compiler',function()
    if arm.state['PC'].value~=0x2a4a6c then return end
    local early=machine.time:as_double()<5
    if early then counts.compiler_early=counts.compiler_early+1 end
    if late() then counts.compiler=counts.compiler+1 end
    if (early and counts.compiler_early<=8) or late() then
        machine:logerror(string.format('nsm1_tone_boundary_compiler: field=%08x value=%08x commit=%08x caller=%08x t=%.9f observer_writes=0\n',arm.state['R0'].value,arm.state['R1'].value,arm.state['R2'].value,arm.state['R14'].value,machine.time:as_double()))
    end
end)
taps[#taps+1]=arm.spaces.program:install_write_tap(0x10000,0x9ffff,'nsm1_tone_mcu_all_mirrors',function(address,value,mask)
    local canonical=address & ~0x8f000
    if canonical<0x10000 or canonical>0x10fff then return end
    if machine.time:as_double()<5 then
        counts.mcu_early=counts.mcu_early+1
        if counts.mcu_early<=8 then machine:logerror(string.format('nsm1_tone_boundary_early: address=%08x value=%08x mask=%08x t=%.9f observer_writes=0\n',address,value,mask,machine.time:as_double())) end
    end
    if not late() then return end
    counts.mcu=counts.mcu+1
    assert(counts.mcu<8192,'late MCU shared trace cap')
    machine:logerror(string.format('nsm1_tone_boundary_mcu: address=%08x canonical=%08x value=%08x mask=%08x pc=%08x lr=%08x t=%.9f observer_writes=0\n',address,canonical,value,mask,arm.state['PC'].value,arm.state['R14'].value,machine.time:as_double()))
end)
taps[#taps+1]=dsp.spaces.data:install_write_tap(0x800,0xfff,'nsm1_tone_dsp_shared',function(address,value,mask)
    if not late() then return end
    counts.dsp=counts.dsp+1
    assert(counts.dsp<8192,'late DSP shared trace cap')
    machine:logerror(string.format('nsm1_tone_boundary_dsp: address=%04x value=%04x mask=%04x pc=%04x t=%.9f observer_writes=0\n',address,value,mask,dsp.state['PC'].value,machine.time:as_double()))
end)
taps[#taps+1]=dsp.spaces.io:install_write_tap(0,0xffff,'nsm1_tone_dsp_io',function(address,value,mask)
    if not late() then return end
    counts.io=counts.io+1
    assert(counts.io<16384,'late DSP IO trace cap')
    machine:logerror(string.format('nsm1_tone_boundary_io: address=%04x value=%04x mask=%04x pc=%04x t=%.9f observer_writes=0\n',address,value,mask,dsp.state['PC'].value,machine.time:as_double()))
end)
_G.nsm1_tone_boundary_taps=taps
local actions=coroutine.create(function()
    dofile(directory..'fixture_deadline.lua')(91.5)
    machine:logerror(string.format('nsm1_tone_boundary_summary: start=85 end=91.5 mcu_early=%d mcu=%d dsp=%d io=%d compiler_early=%d compiler=%d observer_writes=0\n',counts.mcu_early,counts.mcu,counts.dsp,counts.io,counts.compiler_early,counts.compiler))
end)
assert(coroutine.resume(actions))
