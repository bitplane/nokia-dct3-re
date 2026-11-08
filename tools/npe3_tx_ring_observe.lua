-- Own MCU ring publication only. Never dequeue, acknowledge or deliver.
local machine=manager.machine
local cpu=assert(machine.devices[':maincpu'])
local space=cpu.spaces.program
local commits,invalid=0,0
local finish=machine.system.name=='npe3ap5' and 24 or 12
local cap=machine.system.name=='npe3ap5' and 256 or 128
if machine.system.name=='npe3ap5' then
    finish=tonumber(os.getenv('NPE3_NATIVE_SECONDS') or '24')
    assert(finish and finish>=24 and finish<=120 and finish%1==0)
    if finish>24 then cap=2048 end
end
local begin=machine.system.name=='npe3ap5' and 0 or 2
local tap=space:install_write_tap(0x100a4,0x100a7,'npe3_mcu_tx_ring',function(address,data,mask)
    local now=machine.time:as_double()
    if address~=0x100a4 or now<begin or now>finish then return end
    local old=space:read_u32(address)
    local previous=(old>>16)&0xffff
    local producer=(((old&~mask)|(data&mask))>>16)&0xffff
    if producer==previous then return end
    if previous>=82 or producer>=82 then
        invalid=invalid+1
        machine:logerror(string.format('npe3_mcu_tx_invalid: previous=%04x producer=%04x t=%.9f observer_writes=0\n',previous,producer,now))
        return
    end
    commits=commits+1
    if commits>cap then return end
    local words,cursor={},previous
    while cursor~=producer do
        words[#words+1]=string.format('%04x',space:read_u16(0x10000+2*cursor))
        cursor=(cursor+1)%82
    end
    machine:logerror(string.format('npe3_mcu_tx_commit: sequence=%d t=%.9f previous=%04x producer=%04x words=%s pc_context=%08x observer_writes=0\n',commits,now,previous,producer,table.concat(words,','),cpu.state.PC.value))
end)
local stop=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('npe3_mcu_tx_complete: commits=%d cap=%d invalid=%d begin=%d end=%d observer_writes=0\n',commits,cap,invalid,begin,finish))
end)
_G.npe3_mcu_tx_observer={tap,stop}
