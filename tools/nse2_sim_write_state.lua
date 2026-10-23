-- Save after a real APDU header/partial-body flush; observation only.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
local machine=manager.machine
local space=machine.devices[':maincpu'].spaces.program
local capture=dofile(directory..'arm_architecture_snapshot.lua')
local card=machine.devices[':sim_card']
local simi=machine.devices[':simi']
local sms=_G.nse2_sim_state_sms==true
local puk=_G.nse2_sim_state_puk==true
local pin=_G.nse2_sim_state_pin==true or puk
local buffered=_G.nse2_sim_state_buffered==true
local status_pending=_G.nse2_sim_state_status_pending==true
assert(not buffered or (pin and not puk),'buffered checkpoint requires PIN VERIFY')
assert(not status_pending or (pin and not puk and not buffered),'status checkpoint requires PIN VERIFY')
local pin_retries=_G.nse2_pin_state_retries or (puk and 0 or 3)
assert((puk and pin_retries==0) or pin_retries==1 or pin_retries==3,'unreviewed PIN checkpoint input')
assert(not (sms and pin),'one SIM transaction per fixture')
local length=puk and 16 or pin and 8 or sms and 176 or 32
local partial=status_pending and 8 or buffered and 5 or sms and 60 or 0
local capture_time=puk and 25 or pin and 9 or sms and 26.5 or 28.3
local function item(device,name)
    local index=assert(device.items['0/'..name],'missing saved item '..name)
    return emu.item(index)
end
local function hex(value)
    return value:gsub('.',function(byte) return string.format('%02x',byte:byte()) end)
end
local function record()
    if pin then
        local value=hex(item(card,'m_chv_attempts'):read_block(0,2))
        return puk and (value..hex(item(card,'m_chv'):read_block(0,16))) or value
    end
    return hex(item(card,sms and 'm_sms' or 'm_adn'):read_block(0,length))
end
local function fields(device,names)
    local result={}
    for _,name in ipairs(names) do result[#result+1]=name..':'..string.format('%x',item(device,name):read(0)) end
    return table.concat(result,',')
end
local pending,requested,anchor,phase,saved
local tail=''
local sent=0
local traffic={reference={},restored={}}
local retained={}
local function observe(kind,value)
    if not anchor or machine.time:as_double()>=anchor+.5 then return end
    local list=traffic[phase]
    assert(#list<2048,'SIM replay overflow')
    list[#list+1]=string.format('%.9f:%s:%02x',machine.time:as_double()-anchor,kind,value)
end
retained[#retained+1]=space:install_write_tap(0x20034,0x20037,'sim_state_tx',function(_,value,mask)
    if mask&0xff00==0 then return end
    local byte=(value>>8)&0xff
    tail=(tail..string.char(byte)):sub(-5)
    if pending then sent=sent+1 end
    observe('tx',byte)
    local header=puk and string.char(0xa0,0x2c,0,0,16) or pin and string.char(0xa0,0x20,0,1,8) or string.char(0xa0,0xdc,1,4,length)
    if not requested and tail==header then pending=true end
    -- Save scheduling reaches the next TX instruction; verify the resulting
    -- FIFO count in pre-save rather than claiming the request-time count.
    if buffered and pending and sent==partial-1 and not requested then
        requested=true
        machine:logerror(string.format('nse2_sim_state: requested=1 sent=%d t=%.9f\n',sent,machine.time:as_double()))
        machine:save('nse2_sim_pending')
    end
end)
retained[#retained+1]=space:install_write_tap(0x2003c,0x2003f,'sim_state_flush',function(_,value,mask)
    if not buffered and pending and sent==partial and not requested and mask&0xff00~=0 and ((value>>8)&0xff)==0 then
        requested=true
        machine:logerror(string.format('nse2_sim_state: requested=1 sent=%d t=%.9f\n',sent,machine.time:as_double()))
        machine:save('nse2_sim_pending')
    end
end)
retained[#retained+1]=space:install_read_tap(0x20034,0x20037,'sim_state_rx',function(_,value,mask)
    if mask&0xff~=0 then observe('rx',value&0xff) end
end)
local function snapshot(label)
    local s=capture(machine,0x10fffc)
    local protocol=fields(card,{'m_ins','m_tx_len','m_tx_expected','m_receiving_body','m_selected_file'})
    local controller=fields(simi,{'m_rx_count','m_rx_ready','m_iir','m_tx_ready_pending','m_uart_tx_count'})
    local stored=record()
    local free=pin and string.format('%02x03',pin_retries-(status_pending and 1 or 0)) or sms and ('00'..string.rep('ff',175)) or string.rep('ff',32)
    if puk then free=free..'31323334ffffffff35363738ffffffff' end
    assert(stored==free,'SIM persistent state differs at checkpoint')
    local body=partial>0 and hex(item(buffered and simi or card,buffered and 'm_uart_tx_fifo' or 'm_tx'):read_block(0,partial)) or ''
    local result=string.format('t=%.9f pc=%08x sp=%08x ram=%08x cpu=%s card=%s simi=%s record=%s partial=%s',s.time,s.pc,s.sp,s.ram,s.cpu,protocol,controller,stored,body)
    machine:logerror('nse2_sim_state: phase='..label..' '..result..'\n')
    return result
end
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    assert(requested and sent==partial,string.format('SIM checkpoint body count differs: actual=%d expected=%d',sent,partial))
    assert(machine.time:as_double()>(puk and 18.2 or pin and 7.6 or sms and 25.9 or 27.57),'submit contact boundary differs')
    saved=snapshot('saved')
    anchor,phase=machine.time:as_double(),'reference'
end)
local referenced,restored,complete=false,false,false
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    assert(snapshot('restored')==saved,'SIM checkpoint architecture differs')
    phase='restored'
end)
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if phase=='reference' and not referenced and now>=capture_time then
        referenced=true
        machine:logerror('nse2_sim_state: committed=reference record='..record()..'\n')
        machine.screens[':screen']:snapshot('nse2_sim_state_reference.png')
    elseif phase=='reference' and referenced and now>=capture_time+.1 then
        machine:load('nse2_sim_pending')
    elseif phase=='restored' and not restored and now>=capture_time then
        restored=true
        machine:logerror('nse2_sim_state: committed=restored record='..record()..'\n')
        machine.screens[':screen']:snapshot('nse2_sim_state_restored.png')
        local file=assert(io.open('nse2_sim_state_events.log','w'))
        for _,label in ipairs({'reference','restored'}) do
            for i,event in ipairs(traffic[label]) do file:write(label..':'..i..':'..event..'\n') end
        end
        file:close()
        assert(#traffic.reference>0 and #traffic.reference==#traffic.restored,'SIM replay count differs')
        for i,event in ipairs(traffic.reference) do assert(event==traffic.restored[i],'SIM replay mismatch '..i) end
        complete=true
        machine:logerror(string.format('nse2_sim_state: completed=1 reference=%d restored=%d sent_before_save=%d\n',#traffic.reference,#traffic.restored,partial))
    end
end)
retained[#retained+1]=emu.add_machine_stop_notifier(function() if not complete then machine:logerror('nse2_sim_state: FAIL incomplete\n') end end)
_G.nse2_sim_state=retained

if puk then _G.nse2_puk_replay_on_load=true
elseif pin then _G.nse2_pin_replay_on_load=true
elseif sms then _G.nse2_sms_save_hold=.07 else _G.nse2_phonebook_save_hold=.07 end
dofile(directory..(puk and 'nse2_puk_fixture.lua' or pin and 'nse2_pin_wrong_fixture.lua' or sms and 'nse2_sms_fixture.lua' or 'nse2_phonebook_fixture.lua'))

