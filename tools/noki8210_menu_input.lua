-- Own NSM-3 physical input probe, not an injected UI event.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_staged_observe.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local mask_tap = cpu.spaces['program']:install_write_tap(0x20068, 0x2006b,
    '8210_column_mask', function(address, data, mask)
        machine:logerror(string.format(
            '8210_column_mask_write: address=%08x data=%08x mask=%08x pc=%08x t=%.6f\n',
            address, data, mask, cpu.state['PC'].value, machine.time:as_double()))
    end)
_G.noki8210_column_mask_tap = mask_tap
local checksum_tap = cpu.spaces['program']:install_write_tap(0x11ec6c, 0x11ec6f,
    '8210_nv_checksum', function(address, data, mask)
        machine:logerror(string.format(
            '8210_nv_checksum_write: data=%08x mask=%08x pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x source_next=%08x t=%.6f\n',
            data, mask, cpu.state['PC'].value, cpu.state['R14'].value,
            cpu.state['R0'].value, cpu.state['R1'].value,
            cpu.state['R2'].value, cpu.state['R4'].value, machine.time:as_double()))
    end)
_G.noki8210_nv_checksum_tap = checksum_tap
cpu.debug:bpset(0x2eca08, 'r0<=11ec6c && r0+r2>11ec6c',
    'logerror "8210_nv_flash_copy: destination=%08x source=%08x length=%04x caller=%08x\\n",r0,r1,r2,r14;g')
local service_tap = cpu.spaces['program']:install_write_tap(0x13fde0, 0x13fde3,
    '8210_service_flag', function(address, data, mask)
        machine:logerror(string.format(
            '8210_service_flag_write: data=%08x mask=%08x pc=%08x t=%.6f\n',
            data, mask, cpu.state['PC'].value, machine.time:as_double()))
        if cpu.state['PC'].value == 0x240c00 and mask == 0x00ff0000 then
            local space = cpu.spaces['program']
            local base = 0x11ea18
            local sum = 0
            for offset = 0x120, 0x253 do
                if offset ~= 0x154 and offset ~= 0x155 then
                    sum = (sum + space:read_u8(base + offset)) & 0xffff
                end
            end
            local function word(offset)
                return (space:read_u8(base + offset) << 8) |
                    space:read_u8(base + offset + 1)
            end
            machine:logerror(string.format(
                '8210_nv_cache_check: computed=%04x stored=%04x companion=%04x\n',
                sum, word(0x254), word(0x170)))
            local file = assert(io.open('8210_nv_cache.bin', 'wb'))
            for offset = 0, 0x7fff do
                file:write(string.char(space:read_u8(base + offset)))
            end
            file:close()
        end
    end)
_G.noki8210_service_flag_tap = service_tap
cpu.debug:bpset(0x287e48, 'temp0<64',
    'temp0=temp0+1;logerror "8210_task_resume: task=%02x caller=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x2418d6, nil,
    'logerror "8210_mode_release: mode=%02x argument=%02x caller=%08x\\n",r0,r1,r14;g')
-- Branch destination: straight-line ARM fetch hooks may skip mid-block PCs.
cpu.debug:bpset(0x240bf6, nil,
    'logerror "8210_nv_sum_check: computed=%04x stored=%04x companion=%04x\\n",r9,w@(sp+4),w@(sp+6);g')
cpu.debug:bpset(0x2fa99a, nil,
    'logerror "8210_status_dispatch: status=%04x caller=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x307df4, nil,
    'logerror "8210_keypad_decoded: key=%02x\\n",r0;g')
cpu.debug:bpset(0x30aa88, 'temp5<16',
    'temp5=temp5+1;logerror "8210_input_lifecycle: mode=%04x caller=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x305a3a, 'temp4<16',
    'temp4=temp4+1;logerror "8210_keypad_mask: caller=%08x\\n",r14;g')
cpu.debug:bpset(0x305a5c, 'temp3<16',
    'temp3=temp3+1;logerror "8210_keypad_enable: caller=%08x\\n",r14;g')
cpu.debug:bpset(0x307df6, 'temp2<16',
    'temp2=temp2+1;logerror "8210_keyboard_init: caller=%08x\\n",r14;g')
cpu.debug:bpset(0x2885ac, 'r0==1 && temp1<128',
    'temp1=temp1+1;logerror "8210_startup_post: code=%04x caller=%08x\\n",r1,r14;g')
cpu.debug:bpset(0x24a8e4, 'temp8<64',
    'temp8=temp8+1;logerror "8210_readiness_input: code=%02x caller=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x24a9b0, 'temp7<64',
    'temp7=temp7+1;logerror "8210_readiness_flags: values=%02x%02x%02x%02x%02x%02x%02x%02x%02x\\n",b@137e44,b@137e45,b@137e46,b@137e47,b@137e48,b@137e49,b@137e4a,b@137e4b,b@137e4c;g')
local input = coroutine.create(function()
    if _G.noki8210_observe_only then return end
    if not emu.wait(12) then return end
    local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
    machine:logerror('8210_menu_physical: press=1\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('8210_after_menu.png')
end)
_G.noki8210_menu_input = input
assert(coroutine.resume(input))
