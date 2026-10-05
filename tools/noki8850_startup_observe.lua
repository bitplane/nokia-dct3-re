-- Read-only CPU probes plus one raw physical matrix press, not UI acceptance.
-- Invoke with -debug -debugger none. ARM debugger expressions use r14, not lr.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
assert(cpu.debug, "8850 startup probes require the MAME debugger")
cpu.debug:bpset(0x2408c8, nil,
    'logerror "8850_checksum_entry r14=%08x\\n",r14;g')
cpu.debug:bpset(0x240b92, nil,
    'logerror "8850_checksum_compare computed=%04x stored=%04x\\n",r1,r0;g')
cpu.debug:bpset(0x240dbc, nil,
    'logerror "8850_service_reply class=%02x command=%02x status=%02x\\n",b@(r0+3),b@(r0+8),b@(r0+9);g')
cpu.debug:bpset(0x28846c, "temp1<200",
    'temp1=temp1+1;logerror "8850_queue_publish destination=%08x source=%08x r14=%08x\\n",r0,r1,r14;g')
cpu.debug:bpset(0x3054ac, nil,
    'logerror "8850_keypad_disable r14=%08x\\n",r14;g')
cpu.debug:bpset(0x2885bc, "r0==1 && temp2<200",
    'temp2=temp2+1;logerror "8850_task1_post report=%08x r14=%08x\\n",r1,r14;g')
cpu.debug:bpset(0x2a1be8, nil,
    'logerror "8850_startup_receive report=%08x\\n",r0;g')
cpu.debug:bpset(0x2a0dde, "temp3<200",
    'temp3=temp3+1;logerror "8850_task1_receive report=%08x r14=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x2a1a10, "temp4<200",
    'temp4=temp4+1;logerror "8850_startup_dispatch report=%08x state=%04x base=%08x\\n",r0,w@(r4+4),r4;g')
cpu.debug:bpset(0x2a1c96, "temp5<200",
    'temp5=temp5+1;logerror "8850_startup_check power=%02x reports=%02x\\n",b@13ff00,b@137fdd;g')
cpu.debug:bpset(0x244caa, nil,
    'logerror "8850_report14_predecessor pc=%08x\\n",pc;g')
cpu.debug:bpset(0x2ff870, nil,
    'logerror "8850_report14_stub r14=%08x\\n",r14;g')
cpu.debug:bpset(0x2463a4, "temp6<200",
    'temp6=temp6+1;logerror "8850_report14_owner_receive event=%08x state=%04x base=%08x count=%02x init=%02x flag=%02x\\n",r0,w@(r6+1c),r6,b@(r6+4),b@(r6+a),b@(r6+d);g')
cpu.debug:bpset(0x245c28, nil,
    'logerror "8850_owner_status_read value=%08x\\n",r0;g')
cpu.debug:bpset(0x2429a6, nil,
    'logerror "8850_channel_map_receive command=%02x length=%02x\\n",b@(r0+8),b@(r0+5);g')
cpu.debug:bpset(0x304a72, nil,
    'logerror "8850_channel_map_apply source=%08x mode=%02x node=%02x kind=%02x\\n",r3,r1,r0,r2;g')
cpu.debug:bpset(0x304a4a, "temp7<100",
    'temp7=temp7+1;logerror "8850_channel_available resource=%08x\\n",r0;g')
cpu.debug:bpset(0x2d1140, nil,
    'logerror "8850_ui_start r14=%08x\\n",r14;g')
cpu.debug:bpset(0x3014b6, "temp8<100",
    'temp8=temp8+1;logerror "8850_ui_catalogue input=%08x r14=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x2886c0, "b@1115d2==5 && temp0<100",
    'temp0=temp0+1;logerror "8850_task5_receive_entry r14=%08x\\n",r14;g')
cpu.debug:bpset(0x30134e, "temp9<100",
    'temp9=temp9+1;logerror "8850_catalogue_receive message=%08x input=%04x\\n",r0,w@r0;g')
for _, address in ipairs({0x300aae, 0x2fce32, 0x27bd84, 0x270f00,
        0x2c3d38, 0x271bd8, 0x266730}) do
    cpu.debug:bpset(address, "r0==731 || r0==735",
        string.format('logerror "8850_ui_filter entry=%08x input=%%04x\\n",r0;g', address))
end
cpu.debug:bpset(0x268a3c, nil,
    'logerror "8850_ui_735_handler entry=268a3c\\n";g')
cpu.debug:bpset(0x301564, "temp1<250",
    'temp1=temp1+1;logerror "8850_catalogue_internal input=%08x r14=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x287a0e, "r0==51",
    'logerror "8850_ui_timer51_arm delay=%08x r14=%08x\\n",r1,r14;g')
cpu.debug:bpset(0x300894, "(r0>=247 && r0<=263) || r0==621",
    'logerror "8850_startup_transition record=%04x selector=%02x raw_slot=%02x\\n",r0,b@(325b80+r0*8),b@(13fc00+b@(325b80+r0*8));g')
cpu.debug:bpset(0x303214, nil,
    'logerror "8850_virtual_db source=%02x\\n",b@13805a;g')
cpu.debug:go()

local mask_writes = 0
local mask_tap = cpu.spaces["program"]:install_write_tap(0x20068, 0x2006b,
    "8850_keypad_control", function(address, data, mask)
        if mask_writes >= 32 then return end
        mask_writes = mask_writes + 1
        machine:logerror(string.format(
            "8850_keypad_control: address=%08x data=%08x mask=%08x pc=%08x t=%.6f\n",
            address, data, mask, cpu.state["PC"].value, machine.time:as_double()))
    end)
_G.noki8850_keypad_control_tap = mask_tap

local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki8850_frontier_observe.lua")

local input = coroutine.create(function()
    if not emu.wait(5) then return end
    -- Host Menu is column 3/bit 4; its 8850 semantic key is not established.
    local key = assert(machine.ioport.ports[":COL.3"].fields["Menu"])
    key:set_value(1)
    machine:logerror("8850_matrix_press: column=3 host_bit=10\n")
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    if not emu.wait(0.85) then return end
    machine.screens[":screen"]:snapshot("8850_after_matrix.png")
end)
_G.noki8850_startup_input = input
assert(coroutine.resume(input))
