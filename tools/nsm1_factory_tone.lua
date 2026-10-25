-- Own reset-bank lifecycle has no phone-lock prompt; never type into idle.
_G.nsm1_toolkit_skip_phone_authentication=true
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_tone_boundary.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
taps[#taps+1]=memory:install_read_tap(0x2c005c,0x2c005f,'nsm1_factory_tone_constructor',function()
    local now=machine.time:as_double()
    if cpu.state.PC.value~=0x2c005e or now<85 or now>=87 or cpu.state.R2.value~=0x0a then return end
    machine:logerror(string.format('nsm1_factory_tone_constructor: mode=%x tone=%x caller=%x t=%.9f observer_writes=0\n',cpu.state.R0.value,cpu.state.R2.value,cpu.state.R14.value,now))
end)
taps[#taps+1]=memory:install_read_tap(0x2a8748,0x2a874b,'nsm1_factory_tone_admission',function()
    local now=machine.time:as_double()
    if cpu.state.PC.value~=0x2a874a or now<85 or now>=87 or memory:read_u32(0x1118d4)~=0x2db038 then return end
    machine:logerror(string.format('nsm1_factory_tone_admission: descriptor=%08x enabled=%02x setting=%02x task=%02x t=%.9f observer_writes=0\n',memory:read_u32(0x1118d4),memory:read_u8(0x111922),memory:read_u8(0x11fc07),memory:read_u8(0x100022),now))
end)
_G.nsm1_factory_tone_taps=taps
local counts={early=0,late=0}
local dsp=machine.devices[':dsp_c54x:cpu']
for _,range in ipairs({{0x20,0x23},{0x30,0x33}}) do
    taps[#taps+1]=dsp.spaces.data:install_write_tap(range[1],range[2],'nsm1_factory_wave_'..range[1],function(address,data,mask)
        local now=machine.time:as_double()
        local phase=now<5 and 'early' or (now>=85 and now<91.5 and 'late' or nil)
        if not phase then return end
        counts[phase]=counts[phase]+1
        assert(counts.late<65536,'serial-data observation cap')
        if phase=='late' or counts.early<=16 then
            machine:logerror(string.format('nsm1_factory_wave_data: phase=%s address=%04x value=%04x mask=%04x pc=%04x t=%.9f observer_writes=0\n',phase,address,data,mask,dsp.state.PC.value,now))
        end
    end)
end
local serial=coroutine.create(function()
    dofile(directory..'fixture_deadline.lua')(91.5)
    -- BSPC/BSPCE readback is non-destructive; never sample BDRR here.
    machine:logerror(string.format('nsm1_factory_wave_summary: early=%d late=%d control=%04x extension=%04x observer_writes=0\n',counts.early,counts.late,dsp.spaces.data:read_u16(0x22),dsp.spaces.data:read_u16(0x23)))
end)
assert(coroutine.resume(serial))
