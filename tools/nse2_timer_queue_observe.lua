-- Own timer-1d descriptor and queue selection; observation only.
local machine = manager.machine
assert(machine.system.name == 'nse2r4v')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local function active_lr()
    local mode = cpu.state['CPSR'].value & 0x1f
    local name = ({[0x11]='FR14', [0x12]='IR14', [0x13]='SR14', [0x17]='AR14', [0x1b]='UR14'})[mode] or 'R14'
    return mode, cpu.state[name].value
end
_G.nse2_timer_queue_taps = {}
local selections = 0
_G.nse2_timer_queue_taps[1] = space:install_read_tap(0x254c90, 0x254c93, 'nse2_timer_queue_select', function()
    if cpu.state['R15'].value ~= 0x254c92 or cpu.state['R0'].value ~= 0x100294 then return end
    selections = selections + 1
    if selections > 16 then return end
    machine:logerror(string.format('nse2_timer_queue_select: node_flags=%02x global_flags=%02x selected_base=%08x t=%.9f observer_writes=0\n',
        space:read_u8(0x10029b), space:read_u8(0x100024), cpu.state['R3'].value,
        machine.time:as_double()))
end)
for _, site in ipairs({{0x254c7c, 'insert'}, {0x254f78, 'expire'}}) do
    local address, kind = site[1], site[2]
    local count = 0
    _G.nse2_timer_queue_taps[#_G.nse2_timer_queue_taps + 1] =
        space:install_read_tap(address & ~3, (address & ~3) + 3, 'nse2_timer_queue_' .. kind, function()
            if cpu.state['R15'].value ~= address or cpu.state['R0'].value ~= 0x100294 then return end
            count = count + 1
            if count > 16 then return end
            local mode, lr = active_lr()
            local r8_name = mode == 0x11 and 'FR8' or 'R8'
            machine:logerror(string.format('nse2_timer_queue: kind=%s ticks=%04x node_flags=%02x global_flags=%02x head10=%08x head28=%08x mode=%02x active_lr=%08x user_lr=%08x r4=%04x r6=%04x active_r8=%04x t=%.9f observer_writes=0\n',
                kind, kind == 'insert' and cpu.state['R1'].value or space:read_u16(0x100298),
                space:read_u8(0x10029b), space:read_u8(0x100024),
                space:read_u32(0x100030), space:read_u32(0x100048),
                mode, lr, cpu.state['R14'].value,
                cpu.state['R4'].value, cpu.state['R6'].value, cpu.state[r8_name].value,
                machine.time:as_double()))
        end)
end
