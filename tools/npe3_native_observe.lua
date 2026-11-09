-- Own NPE-3 MCU sites and acquired mask entrypoints; observation only.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'dct3_model_scout.lua')
local machine=manager.machine
local reference=machine.system.name=='npe3ref' or machine.system.name=='npe3afs' or machine.system.name=='npe3ad0' or machine.system.name=='npe3cp9' or machine.system.name=='npe3zp9' or machine.system.name=='npe3bp9' or machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5' or machine.system.name=='npe3ap5'
assert(reference or machine.system.name=='npe3r6')
assert(not machine.devices[':dsp_hle'] and not machine.devices[':dsp_staged'])
local arm=assert(machine.devices[':maincpu'])
local dsp=assert(machine.devices[':dsp_c54x:cpu'])
machine:logerror(string.format('npe3_native_composition: hle=0 staged=0 clock_policy=%d fitted_mask_claim=0 observer_writes=0\n',reference and 45500000 or 13000000))
if reference then
    machine:logerror('npe3_native_reference_clock: source=8210_documented_rate fitted_clock_claim=0 experiment=1 observer_writes=0\n')
end
machine:logerror('npe3_native_control_contract: status=10 release=04 execution=01 readback=05 observer_writes=0\n')
machine.debugger:command('temp6=0')
machine.debugger:command('temp7=0')
arm.debug:bpset(0x4fad30,'temp7<28 && (temp8=r0)>=0',
    'temp7=temp7+1;logerror "npe3_native_key: value=%02x observer_writes=0\\n",temp8;g')
arm.debug:bpset(0x426cca,'1',
    'logerror "npe3_native_verifier: result0=%04x result1=%04x observer_writes=0\\n",maincpu.w@10000,maincpu.w@10002;g')
arm.debug:bpset(0x302a52,'temp6<8 && (temp9=r4)>=0',
    'temp6=temp6+1;logerror "npe3_native_selftest: command=%02x faults=%02x flag=%02x observer_writes=0\\n",maincpu.b@(temp9+8),maincpu.b@(temp9+9),maincpu.b@17fd99;g')
for index,site in ipairs({{0xff80,'reset'},{0x0f00,'loader'},{0x2c75,'resident'}}) do
    local counter='temp'..(index-1)
    machine.debugger:command(counter..'=0')
    dsp.debug:bpset(site[1],counter..'<10',counter..'='..counter..
        '+1;logerror "npe3_native_entry: kind='..site[2]..' ordinal=%d observer_writes=0\\n",'..counter..';g')
end
local captures={}
for _,address in ipairs({0x4dc0e4,0x426c36,0x50096a}) do
    arm.debug:bpset(address,'1',
        'logerror "npe3_native_mcu_restart: pc=%08x lr=%08x argument=%08x activity=%02x misses=%02x observer_writes=0\\n",pc,r14,r0,b@17fda3,b@17fd9e;g')
end
local control_writes=0
_G.npe3_native_control=dsp.spaces.io:install_write_tap(0x36,0x36,'npe3_native_control',function(_,value)
    control_writes=control_writes+1
    if control_writes<=32 then
        machine:logerror(string.format('npe3_native_control: sequence=%d t=%.9f value=%04x owner=%06x\n',
            control_writes,machine.time:as_double(),value,dsp.state.EXECPC.value))
    end
end)
local missing_program=0
_G.npe3_native_coverage=dsp.spaces.program:install_read_tap(0x80,0x1fff,'npe3_native_coverage',function()
    if dsp.state.PMST.value&0x20==0 then missing_program=missing_program+1 end
end)
local upper_reads=0
_G.npe3_native_upper_coverage=dsp.spaces.program:install_read_tap(0x10000,0x7fffff,'npe3_native_upper_coverage',function(address,value)
    -- This composition maps only page0. Count escapes even if unbacked words
    -- happen to decode legally; terminal ILLEGAL alone is insufficient.
    missing_program=missing_program+1;upper_reads=upper_reads+1
    if upper_reads==1 then
        machine:logerror(string.format('npe3_native_program_escape: t=%.9f address=%06x value=%04x pc=%06x readpc=%06x execpc=%06x xpc=%02x sp=%04x observer_writes=0\n',
            machine.time:as_double(),address,value,dsp.state.GENPC.value,dsp.state.READPC.value,
            dsp.state.EXECPC.value,dsp.state.XPC.value,dsp.state.SP.value))
    end
end)
_G.npe3_native_loader=dsp.spaces.program:install_read_tap(0x0a00,0x0a00,'npe3_native_loader',function()
    if captures.loader then return end
    captures.loader=true
    local file=assert(io.open('npe3_loader2.be16','wb'))
    for address=0xa00,0xa00+628 do file:write(string.pack('>I2',dsp.spaces.data:read_u16(address))) end
    file:close()
end)
_G.npe3_native_stop=emu.add_machine_stop_notifier(function()
    for _,space in ipairs({'program','data'}) do
        local file=assert(io.open('npe3_terminal_'..space..'.be16','wb'))
        for address=0,65535 do file:write(string.pack('>I2',dsp.spaces[space]:read_u16(address))) end
        file:close()
    end
    machine:logerror(string.format('npe3_native_terminal: t=%.6f arm=%08x dsp=%06x illegal=%u idle=%u pmst=%04x observer_writes=0\n',
        machine.time:as_double(),arm.state.GENPC.value,dsp.state.GENPC.value,
        dsp.state.ILLEGAL.value,dsp.state.IDLE.value,dsp.state.PMST.value))
    machine:logerror(string.format('npe3_native_coverage: missing_program_reads=%d observer_writes=0\n',missing_program))
end)
local scenario=os.getenv('NPE3_NATIVE_SCENARIO') or 'menu'
local input_at=tonumber(os.getenv('NPE3_NATIVE_INPUT_AT') or '20')
assert(input_at and input_at>=20 and input_at<=100)
if scenario~='menu' then
    _G.noki6210_phonebook_start=input_at
    dofile(directory..(scenario=='phonebook' and 'noki6210_phonebook_input.lua' or 'noki6210_phonebook_read.lua'))
    return
end
_G.npe3_native_input=coroutine.create(function()
    if not emu.wait(input_at) then return end
    machine.screens[':screen']:snapshot('npe3_before_menu.png')
    local name=os.getenv('NPE3_NATIVE_CALL_KEY') or 'Menu'
    local choices={Menu={':COL.1','Left Softkey / Menu'},RightSoftkey={':COL.1','Right Softkey / C'},Send={':COL.0','Send'}}
    local choice=assert(choices[name])
    local key=assert(machine.ioport.ports[choice[1]].fields[choice[2]])
    machine:logerror(string.format('npe3_native_physical: key=%s t=%d observer_writes=0\n',name,input_at))
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0);return end
    key:set_value(0)
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('npe3_after_menu.png')
end)
assert(coroutine.resume(_G.npe3_native_input))
