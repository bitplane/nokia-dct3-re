-- Own NSM-2 v5.31 MCU sites and acquired mask entrypoints; observation only.
-- Invoke with -debug -debugger none. NSM2_NATIVE_SCENARIO selects input:
-- 'boot' (screenshots only) or 'menu' (security code, then Menu).
local machine=manager.machine
assert(machine.system.name=='nsm2r6')
assert(not machine.devices[':dsp_hle'] and not machine.devices[':dsp_staged'])
local arm=assert(machine.devices[':maincpu'])
local dsp=assert(machine.devices[':dsp_c54x:cpu'])
machine:logerror('nsm2_native_composition: hle=0 staged=0 clock_policy=45500000 fitted_mask_claim=0 observer_writes=0\n')
-- Final-result poll 2caeae..2caeb4 leaves when shared word +2 is not ffff.
arm.debug:bpset(0x2caeb6,'1',
    'logerror "nsm2_native_verifier: result0=%04x result1=%04x observer_writes=0\\n",w@r4,w@(r4+2);g')
local scenario=os.getenv('NSM2_NATIVE_SCENARIO') or 'boot'
assert(scenario=='boot' or scenario=='menu')
-- Own class-74 handler: command at +8, fault bits at +9.
arm.debug:bpset(0x240dbc,'1',
    'logerror "8850_service_reply class=%02x command=%02x status=%02x\\n",b@(r0+3),b@(r0+8),b@(r0+9);g')
for _,site in ipairs({{0xff80,'reset'},{0x0f00,'loader'},{0x2c75,'resident'}}) do
    dsp.debug:bpset(site[1],'1','logerror "nsm2_native_entry: kind='..site[2]..' observer_writes=0\\n";g')
end
-- MAD2 register 01 bit 2 requests the MCU software reset.
_G.nsm2_native_reset_tap=arm.spaces.program:install_write_tap(0x20000,0x20003,'nsm2_native_reset',function(address,value,mask)
    if (mask>>16)&0xff~=0 and (value>>16)&0x04~=0 then
        machine:logerror(string.format('nsm2_native_mcu_reset: t=%.6f pc=%08x lr=%08x observer_writes=0\n',
            machine.time:as_double(),arm.state.GENPC.value,arm.state.R14.value))
    end
end)
local captures={}
local missing_program=0
_G.nsm2_native_coverage=dsp.spaces.program:install_read_tap(0x80,0x1fff,'nsm2_native_coverage',function()
    if dsp.state.PMST.value&0x20==0 then missing_program=missing_program+1 end
end)
_G.nsm2_native_loader=dsp.spaces.program:install_read_tap(0x0a00,0x0a00,'nsm2_native_loader',function()
    if captures.loader then return end
    captures.loader=true
    local file=assert(io.open('nsm2_loader2.be16','wb'))
    for address=0xa00,0xa00+612 do file:write(string.pack('>I2',dsp.spaces.data:read_u16(address))) end
    file:close()
end)
_G.nsm2_native_stop=emu.add_machine_stop_notifier(function()
    local file=assert(io.open('nsm2_terminal_program.be16','wb'))
    for address=0,65535 do file:write(string.pack('>I2',dsp.spaces.program:read_u16(address))) end
    file:close()
    machine:logerror(string.format('nsm2_native_terminal: t=%.6f arm=%08x dsp=%06x illegal=%u idle=%u pmst=%04x observer_writes=0\n',
        machine.time:as_double(),arm.state.GENPC.value,dsp.state.GENPC.value,
        dsp.state.ILLEGAL.value,dsp.state.IDLE.value,dsp.state.PMST.value))
    machine:logerror(string.format('nsm2_native_coverage: missing_program_reads=%d observer_writes=0\n',missing_program))
end)

-- One-second markers bracket untimed debugger log lines.
_G.nsm2_native_clock=coroutine.create(function()
    while emu.wait(1) do machine:logerror(string.format('nsm2_native_tick: t=%.0f\n',machine.time:as_double())) end
end)
assert(coroutine.resume(_G.nsm2_native_clock))

local function press(port,name,marker)
    local field=assert(machine.ioport.ports[port].fields[name],name)
    machine:logerror(string.format('%s t=%.3f\n',marker,machine.time:as_double()))
    field:set_value(1)
    if not emu.wait(0.15) then field:set_value(0);return false end
    field:set_value(0)
    return true
end

if scenario=='menu' then
    -- Own decoded-key exits (as noki8850_startup_observe), then the 8850's
    -- reviewed physical security code, Messages/Inbox and Names navigation.
    for _,address in ipairs({0x2a12e0,0x2a12fe,0x2a1358}) do
        arm.debug:bpset(address,'1','logerror "8850_keypad_decoded key=%02x\\n",r0;g')
    end
    _G.nsm2_native_input=coroutine.create(function()
        if not emu.wait(12) then return end
        machine.screens[':screen']:snapshot('8850_security_prompt.png')
        for _,item in ipairs({{2,'Keypad 1'},{3,'Keypad 2'},{4,'Keypad 3'},{2,'Keypad 4'},{3,'Keypad 5'},{1,'Menu'}}) do
            if not press(':COL.'..item[1],item[2],'8850_security_physical: key='..item[2]) then return end
            if not emu.wait(0.35) then return end
        end
        if not emu.wait(8) then return end
        machine.screens[':screen']:snapshot('8850_after_security.png')
        for _,name in ipairs({'messages_menu','inbox'}) do
            if not press(':COL.1','Menu','8850_navigation_physical: action='..name) then return end
            if not emu.wait(0.85) then return end
            machine.screens[':screen']:snapshot('8850_'..name..'.png')
        end
        for index=1,3 do
            if not press(':COL.1','Names / C','8850_navigation_physical: action='..(index==3 and 'names' or 'back')) then return end
            if not emu.wait(0.85) then return end
        end
        machine.screens[':screen']:snapshot('8850_names.png')
    end)
    assert(coroutine.resume(_G.nsm2_native_input))
    return
end
_G.nsm2_native_input=coroutine.create(function()
    for second=2,tonumber(os.getenv('NSM2_NATIVE_SHOTS') or '30'),2 do
        if not emu.wait(2) then return end
        machine.screens[':screen']:snapshot(string.format('nsm2_boot_%02d.png',second))
    end
end)
assert(coroutine.resume(_G.nsm2_native_input))
