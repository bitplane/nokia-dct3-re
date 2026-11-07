-- Finite raw ADC tone bursts, never decoded radio messages or success flags.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_measure.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local writes=0
local adc_reads,active_words=0,0
local sch_raw_words=0
local sch_windows=0
local fcch_state_writes=0
_G.rom6_burst_fcch_state={}
for _,address in ipairs({0x2f65,0x2f6b,0x2f7a,0x2f7b,0x2477,0x247a,0x247b,0x2627}) do
    _G.rom6_burst_fcch_state[#_G.rom6_burst_fcch_state+1]=cpu.spaces.data:install_write_tap(address,address,'rom6_burst_fcch_state',function(at,value)
        local pc=cpu.state.GENPC.value
        if pc<0xb40 or pc>=0xe00 then return end
        fcch_state_writes=fcch_state_writes+1
        if fcch_state_writes>256 then return end
        machine:logerror(string.format('rom6_burst_fcch_state: sequence=%d t=%.9f address=%04x value=%04x pc=%06x reads=%d observer_writes=0\n',
            fcch_state_writes,machine.time:as_double(),at,value,pc,adc_reads))
        if at==0x2627 then
            local function word(address) return cpu.spaces.data:read_u16(address) end
            machine:logerror(string.format('rom6_burst_fcch_fields: t=%.9f word2f65=%04x word2f78=%04x word2f7b=%04x word2f6c=%04x word2f6d=%04x words2f68=%04x,%04x words247a=%04x,%04x observer_writes=0\n',
                machine.time:as_double(),word(0x2f65),word(0x2f78),word(0x2f7b),word(0x2f6c),word(0x2f6d),word(0x2f68),word(0x2f69),word(0x247a),word(0x247b)))
        end
    end)
end
local counter_loads=0
_G.rom6_burst_counter_load=cpu.spaces.io:install_write_tap(0x0d,0x0d,'rom6_burst_counter_load',function(_,value)
    counter_loads=counter_loads+1
    if counter_loads<=32 then
        machine:logerror(string.format('rom6_burst_counter_load: sequence=%d t=%.9f value=%04x pc=%06x reads=%d observer_writes=0\n',
            counter_loads,machine.time:as_double(),value,cpu.state.GENPC.value,adc_reads))
    end
end)
local sch_sample_writes={}
local last_counter,last_counter_time=0,0
_G.rom6_burst_counter=cpu.spaces.io:install_read_tap(0x0d,0x0d,'rom6_burst_counter',function(_,value)
    if sch_windows>0 then return end
    last_counter=value; last_counter_time=machine.time:as_double()
end)
local capture_controls,capture_control_count={},0
_G.rom6_burst_capture_controls={}
for _,port in ipairs({0x03,0x09,0x0a,0x0b,0x21}) do
    _G.rom6_burst_capture_controls[#_G.rom6_burst_capture_controls+1]=cpu.spaces.io:install_write_tap(port,port,'rom6_burst_capture_control',function(_,value)
        if sch_windows>0 then return end
        capture_control_count=capture_control_count+1
        capture_controls[(capture_control_count-1)%128+1]={port=port,value=value,time=machine.time:as_double(),reads=adc_reads,pc=cpu.state.GENPC.value,counter=last_counter,counter_time=last_counter_time}
    end)
end
_G.rom6_burst_sch_samples=cpu.spaces.data:install_write_tap(0xd8,0x197,'rom6_burst_sch_samples',function(address,value)
    if adc_reads==0 or sch_windows>0 then return end
    sch_sample_writes[address]={value=value,time=machine.time:as_double(),reads=adc_reads,pc=cpu.state.GENPC.value,input=cpu.state.AR2.value}
end)
_G.rom6_burst_sch_window=cpu.spaces.data:install_write_tap(0x10,0x10,'rom6_burst_sch_window',function(_,value)
    if value~=0x7737 then return end
    local pc=cpu.state.GENPC.value
    if pc<0x76a7 or pc>0x76d9 then return end
    sch_windows=sch_windows+1
    if sch_windows>40 then return end
    local samples={}
    if sch_windows==1 then
        for index=math.max(1,capture_control_count-127),capture_control_count do
            local item=capture_controls[(index-1)%128+1]
            machine:logerror(string.format('rom6_burst_capture_control: sequence=%d t=%.9f port=%02x value=%04x reads=%d pc=%06x counter=%04x counter_t=%.9f observer_writes=0\n',
                index,item.time,item.port,item.value,item.reads,item.pc,item.counter,item.counter_time))
        end
        for address=0xd8,0x197 do samples[#samples+1]=string.format('%04x',cpu.spaces.data:read_u16(address)) end
        for address=0xd8,0x197 do
            local last=sch_sample_writes[address]
            if last then
                machine:logerror(string.format('rom6_burst_sch_sample_write: address=%04x value=%04x t=%.9f reads=%d pc=%06x input=%04x observer_writes=0\n',
                    address,last.value,last.time,last.reads,last.pc,last.input))
            end
        end
    end
    machine:logerror(string.format('rom6_burst_sch_window: sequence=%d t=%.9f reads=%d pointer=%04x pc=%06x samples=%s observer_writes=0\n',
        sch_windows,machine.time:as_double(),adc_reads,cpu.state.AR3.value,pc,table.concat(samples,',')))
end)
_G.rom6_burst_active_tap=cpu.spaces.io:install_read_tap(0x27,0x27,'rom6_burst_active_samples',function(_,value)
    adc_reads=adc_reads+1
    if machine.system.name=='nsm3afs' and sch_raw_words<1024 then
        local tick=math.floor(machine.time:as_double()*13000000)
        local frame=math.floor(tick/60000)%51
        local sch_frame=frame==1 or frame==11 or frame==21 or frame==31 or frame==41
        -- Only choose an observation window here. The checker reconstructs
        -- conversion time from queue epochs, not this delayed CPU read time.
        if sch_raw_words>0 or (sch_frame and tick%60000<7104 and value~=0) then
            sch_raw_words=sch_raw_words+1
            machine:logerror(string.format('rom6_burst_sch_raw_word: sequence=%d read=%d value=%04x t=%.9f observer_writes=0\n',
                sch_raw_words,adc_reads,value&0xffff,machine.time:as_double()))
        end
    end
    if active_words>=1024 or (active_words==0 and value==0) then return end
    active_words=active_words+1
    machine:logerror(string.format('rom6_burst_active_word: sequence=%d read=%d value=%04x t=%.9f observer_writes=0\n',
        active_words,adc_reads,value&0xffff,machine.time:as_double()))
end)
_G.rom6_burst_state_tap=cpu.spaces.data:install_write_tap(0x3044,0x3044,'rom6_burst_positive_count',function(_,value,mask)
    writes=writes+1
    if writes<=1024 then
        machine:logerror(string.format('rom6_burst_count: sequence=%d t=%.9f value=%04x mask=%04x cpu_pc=%06x observer_writes=0\n',
            writes,machine.time:as_double(),value,mask,cpu.state.GENPC.value))
    end
end)
local commits=0
local block_writes=0
_G.rom6_burst_block_tap=cpu.spaces.data:install_write_tap(0x2f5b,0x2f61,'rom6_burst_block_fields',function(address,value)
    local pc=cpu.state.GENPC.value
    if pc<0x4b00 or pc>=0x5300 then return end
    block_writes=block_writes+1
    if block_writes>256 then return end
    machine:logerror(string.format('rom6_burst_block_field: sequence=%d t=%.9f address=%04x value=%04x pc=%06x observer_writes=0\n',
        block_writes,machine.time:as_double(),address,value,cpu.state.GENPC.value))
    if address==0x2f5b then
        local function word(at) return cpu.spaces.data:read_u16(at) end
        local other=word(0x2f1e)~=0
        local frame=other and 0x2456 or 0x912
        machine:logerror(string.format('rom6_burst_block_sources: sequence=%d channel=%04x selection=%04x crc=%04x bsic=%04x frame=%04x,%04x arfcn=%04x shift=%04x decoded=%04x,%04x,%04x observer_writes=0\n',
            block_writes,word(0x2f1f),word(0x2f1e),word(0x2f20),word(other and 0x2616 or 0x250a),
            word(frame),word(frame+1),word(other and 0x2618 or 0x250e),word(0x2615),word(0x2f80),word(0x2f81),word(0x2f82)))
    end
end)
local crc_writes=0
_G.rom6_burst_crc_tap=cpu.spaces.data:install_write_tap(0x2f20,0x2f20,'rom6_burst_crc_status',function(_,value)
    local pc=cpu.state.GENPC.value
    if pc<0x4b00 or pc>=0x5300 then return end
    crc_writes=crc_writes+1
    if crc_writes>16 then return end
    machine:logerror(string.format('rom6_burst_crc: sequence=%d t=%.9f status=%04x remainder=%04x input=%04x,%04x,%04x pc=%06x observer_writes=0\n',
        crc_writes,machine.time:as_double(),value,cpu.state.A.value&0x3ff,
        cpu.spaces.data:read_u16(0x2f80),cpu.spaces.data:read_u16(0x2f81),cpu.spaces.data:read_u16(0x2f82),pc))
end)
_G.rom6_burst_rx_context=cpu.spaces.data:install_write_tap(0x8e4,0x8e4,'rom6_burst_rx_context',function(_,value)
    if cpu.spaces.data:read_u16(0x8e4)==value then return end
    commits=commits+1
    if commits>16 then return end
    local stack={}
    local sp=cpu.state.SP.value
    if sp>=0x60 and sp<0xfff8 then
        for i=0,7 do stack[#stack+1]=string.format('%04x',cpu.spaces.data:read_u16(sp+i)) end
    end
    machine:logerror(string.format('rom6_burst_rx_context: sequence=%d t=%.9f pc=%06x ar=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x sp=%04x stack=%s a=%010x b=%010x observer_writes=0\n',
        commits,machine.time:as_double(),cpu.state.GENPC.value,cpu.state.AR0.value,cpu.state.AR1.value,cpu.state.AR2.value,cpu.state.AR3.value,
        cpu.state.AR4.value,cpu.state.AR5.value,cpu.state.AR6.value,cpu.state.AR7.value,sp,table.concat(stack,','),cpu.state.A.value,cpu.state.B.value))
end)
_G.rom6_burst_stop=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_burst_complete: count_writes=%d cap=1024 observer_writes=0\n',writes))
    machine:logerror(string.format('rom6_burst_fcch_complete: writes=%d cap=256 observer_writes=0\n',fcch_state_writes))
    machine:logerror(string.format('rom6_burst_counter_load_complete: writes=%d cap=32 observer_writes=0\n',counter_loads))
    machine:logerror(string.format('rom6_burst_active_complete: words=%d cap=1024 reads=%d observer_writes=0\n',active_words,adc_reads))
    if machine.system.name=='nsm3afs' then
        machine:logerror(string.format('rom6_burst_sch_raw_complete: words=%d cap=1024 reads=%d observer_writes=0\n',sch_raw_words,adc_reads))
    end
    machine:logerror(string.format('rom6_burst_sch_complete: windows=%d cap=40 observer_writes=0\n',sch_windows))
    machine:logerror(string.format('rom6_burst_terminal: pc=%06x ar2=%04x ar3=%04x ar4=%04x ar5=%04x a=%010x b=%010x observer_writes=0\n',
        cpu.state.GENPC.value,cpu.state.AR2.value,cpu.state.AR3.value,cpu.state.AR4.value,cpu.state.AR5.value,cpu.state.A.value,cpu.state.B.value))
    machine.debugger.visible_cpu=cpu
    local pc=cpu.state.GENPC.value
    local first=(pc&0x7f0000)|math.max(0,(pc&0xffff)-0x40)
    machine.debugger:command(string.format('dasm rom6_burst_search.asm,%x,c0,1,:dsp_c54x:cpu',first))
    machine.debugger:command('dasm rom6_burst_rx_publisher.asm,2cc0,a0,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_block_constructor.asm,4b00,140,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_block_fields.asm,4d80,220,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_block_result.asm,51c0,100,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_block_decoder.asm,9200,100,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_sch.asm,7600,200,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_sch_entry.asm,7400,200,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_sch_builder.asm,7800,200,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_fcch_search.asm,b40,2c0,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm rom6_burst_capture_program.asm,3d00,350,1,:dsp_c54x:cpu')
end)
