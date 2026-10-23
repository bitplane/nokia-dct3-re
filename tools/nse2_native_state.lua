-- Native/UI save/load observation; only physical input and emulator state APIs.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nse2_security_fixture.lua')
local machine=manager.machine
local capture=dofile(directory..'arm_architecture_snapshot.lua')
local dsp=assert(machine.devices[':dsp_c54x:cpu'])
local anchor,phase
local events={reference={},restored={}}
local retained={}
retained[#retained+1]=dsp.spaces.io:install_write_tap(0,0xffff,'nse2_state_io',function(address,value,mask)
    local now=machine.time:as_double()
    if not anchor or now<anchor or now>=anchor+2.7 then return end
    local list=events[phase]
    assert(#list<16384,'NSE-2 state I/O trace overflow')
    list[#list+1]=string.format('%.9f:%04x:%04x:%04x',now-anchor,address,value,mask)
end)
local function snapshot(label)
    local arm=capture(machine,0x10fffc)
    local registers={}
    for _,name in ipairs({'PC','XPC','A','B','T','TRN','RTN','RC','SP','ST0','ST1','PMST','AR0','AR1','AR2','AR3','AR4','AR5','AR6','AR7','BRC','RSA','REA','BK','IFR','IMR','SWWSR','BSCR','SWCR','TIM','PRD','TCR','IDLE','ILLEGAL','GENPC','CURPC'}) do
        registers[#registers+1]=string.format('%s:%x',name,dsp.state[name].value)
    end
    local data,program=0,0
    -- Exclude mapped DSP registers; checksum only data RAM and program words.
    for address=0x80,0x7fff do data=((data<<5)-data+dsp.spaces.data:read_u16(address))&0xffffffff end
    for address=0,0xffff do program=((program<<5)-program+dsp.spaces.program:read_u16(address))&0xffffffff end
    local result=string.format('t=%.9f pc=%08x sp=%08x ram=%08x cpu=%s dsp=%s data=%08x program=%08x',arm.time,arm.pc,arm.sp,arm.ram,arm.cpu,table.concat(registers,','),data,program)
    machine:logerror('nse2_state: phase='..label..' '..result..'\n')
    return result
end
local saved,complete
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    saved=snapshot('saved')
    anchor,phase=machine.time:as_double(),'reference'
end)
local function menu(label)
    local field
    for _,candidate in pairs(machine.ioport.ports[':COL.1'].fields) do if candidate.mask==4 then field=candidate end end
    assert(field,'NSE-2 Menu contact missing')
    field:set_value(1)
    machine:logerror('nse2_state_contact: phase='..label..' down=1\n')
    assert(emu.wait(.2))
    field:set_value(0)
    machine:logerror('nse2_state_contact: phase='..label..' down=0\n')
    assert(emu.wait(1))
    machine.screens[':screen']:snapshot('nse2_state_'..label..'.png')
end
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    assert(saved and snapshot('restored')==saved,'NSE-2 native architectural restoration differs')
    phase='restored'
    local replay=coroutine.create(function()
        assert(emu.wait(1.5))
        menu('restored')
        assert(#events.reference>0 and #events.reference==#events.restored,'NSE-2 native I/O replay count differs')
        for i,event in ipairs(events.reference) do
            assert(events.restored[i]==event,'NSE-2 native I/O replay differs at '..i)
        end
        machine:logerror(string.format('nse2_state_replay: reference=%d restored=%d window=2.7 exact=1\n',#events.reference,#events.restored))
        complete=true
        machine:logerror('nse2_state: completed=1\n')
    end)
    retained[#retained+1]=replay
    assert(coroutine.resume(replay))
end)
local runner=coroutine.create(function()
    assert(emu.wait(18.5))
    machine:save('nse2_native_ui')
    assert(emu.wait(1.5))
    assert(saved,'NSE-2 save did not execute')
    menu('reference')
    machine:load('nse2_native_ui')
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function() if not complete then machine:logerror('nse2_state: FAIL incomplete\n') end end)
_G.nse2_native_state=retained
assert(coroutine.resume(runner))
