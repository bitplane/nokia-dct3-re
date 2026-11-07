-- Finite raw ADC tone bursts, never decoded radio messages or success flags.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_measure.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local writes=0
local adc_reads,active_words=0,0
_G.rom6_burst_active_tap=cpu.spaces.io:install_read_tap(0x27,0x27,'rom6_burst_active_samples',function(_,value)
    adc_reads=adc_reads+1
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
    machine:logerror(string.format('rom6_burst_active_complete: words=%d cap=1024 reads=%d observer_writes=0\n',active_words,adc_reads))
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
end)
