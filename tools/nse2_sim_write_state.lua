-- Save after real UPDATE header flush, before body TX; observation only.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
local machine=manager.machine
local space=machine.devices[':maincpu'].spaces.program
local capture=dofile(directory..'arm_architecture_snapshot.lua')
local card=machine.devices[':sim_card']
local simi=machine.devices[':simi']
local function item(device,name)
    local index=assert(device.items['0/'..name],'missing saved item '..name)
    return emu.item(index)
end
local function adn()
    return item(card,'m_adn'):read_block(0,32):gsub('.',function(byte) return string.format('%02x',byte:byte()) end)
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
    if not requested and tail==string.char(0xa0,0xdc,1,4,32) then pending=true end
end)
retained[#retained+1]=space:install_write_tap(0x2003c,0x2003f,'sim_state_flush',function(_,value,mask)
    if pending and not requested and mask&0xff00~=0 and ((value>>8)&0xff)==0 then
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
    local record=adn()
    assert(record==string.rep('ff',32),'ADN was not rolled back before commit')
    local result=string.format('t=%.9f pc=%08x sp=%08x ram=%08x cpu=%s card=%s simi=%s adn=%s',s.time,s.pc,s.sp,s.ram,s.cpu,protocol,controller,record)
    machine:logerror('nse2_sim_state: phase='..label..' '..result..'\n')
    return result
end
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    assert(requested and sent==0,'SIM body already transmitted before save')
    assert(machine.time:as_double()>27.57,'Save contact still held')
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
    if phase=='reference' and not referenced and now>=28.3 then
        referenced=true
        machine:logerror('nse2_sim_state: committed=reference adn='..adn()..'\n')
        machine.screens[':screen']:snapshot('nse2_sim_state_reference.png')
    elseif phase=='reference' and referenced and now>=28.4 then
        machine:load('nse2_sim_pending')
    elseif phase=='restored' and not restored and now>=28.3 then
        restored=true
        machine:logerror('nse2_sim_state: committed=restored adn='..adn()..'\n')
        machine.screens[':screen']:snapshot('nse2_sim_state_restored.png')
        assert(#traffic.reference>0 and #traffic.reference==#traffic.restored,'SIM replay count differs')
        for i,event in ipairs(traffic.reference) do assert(event==traffic.restored[i],'SIM replay mismatch '..i) end
        local file=assert(io.open('nse2_sim_state_events.log','w'))
        for _,label in ipairs({'reference','restored'}) do
            for i,event in ipairs(traffic[label]) do file:write(label..':'..i..':'..event..'\n') end
        end
        file:close()
        complete=true
        machine:logerror(string.format('nse2_sim_state: completed=1 reference=%d restored=%d sent_before_save=0\n',#traffic.reference,#traffic.restored))
    end
end)
retained[#retained+1]=emu.add_machine_stop_notifier(function() if not complete then machine:logerror('nse2_sim_state: FAIL incomplete\n') end end)
_G.nse2_sim_state=retained

_G.nse2_phonebook_save_hold=.07
dofile(directory..'nse2_phonebook_fixture.lua')

