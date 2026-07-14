local cpu = manager.machine.devices[":maincpu"]
local screen = manager.machine.screens[":screen"]
local output_dir = os.getenv("NOKI3210_SNAPSHOT_DIR") or ".."
local boot_summary_path = os.getenv("NOKI3210_BOOT_SUMMARY")
local quiet = os.getenv("NOKI3210_LUA_QUIET") == "1"
local exercise_input = os.getenv("NOKI3210_INPUT_EXERCISER_PRESS") ~= "0"

if quiet then
	emu.print_info = function(...)
	end
end

local debug = cpu and cpu.debug or nil
local space = cpu.spaces["program"]
if not debug then
	emu.print_info("noki3210 input exerciser: debugger interface unavailable; running input/snapshot path only")
end
local frames = 0
local lcd_data_writes = 0
local lcd_cmd_writes = 0
local nonzero_lcd_data_writes = 0
local active_fields = {}
local dumped_frames = {}
local lcd_mirror_vram = {}
local lcd_mirror_mode = 0x04
local lcd_mirror_control = 0x00
local lcd_mirror_x = 0
local lcd_mirror_y = 0
local lcd_mirror_dumps = 0
local lcd_mirror_pending = nil
local dumped_nonblank_lcd_screen = false
local flash_write_logs = 0
local dump_screen
local structural = {
	gensio_controls = {},
	ccont_commands = {},
	ccont_bytes = 0,
	ccont_reads = 0,
	eeprom_starts = 0,
	eeprom_signal_writes = 0,
	irq_seen = 0,
	fiq_seen = 0,
	startup_modes = {},
	soft_resets = 0,
	final_current_task = 0,
	final_startup_mode = 0,
	final_startup_event = 0,
	final_startup_flags = 0,
	final_contact_status = 0,
	final_no_sim = 0,
	final_sim_enable = 0,
}
local gensio_control = 0
local ccont_command_phase = true
local genio_signal = 0
local genio_direction = 0
local eeprom_sda = 1
local eeprom_scl = 0

for i = 0, (84 * 6) - 1 do
	lcd_mirror_vram[i] = 0
end

local function field_by_mask(port_tag, mask)
	local port = manager.machine.ioport.ports[port_tag]
	if not port then
		emu.print_error(string.format("missing input port %s", port_tag))
		return nil
	end

	local field = port:field(mask)
	if not field then
		emu.print_error(string.format("missing input field %s:%02x", port_tag, mask))
	end
	return field
end

local enter_field = field_by_mask(":COL.4", 0x08) or field_by_mask("COL.4", 0x08)
local power_field = field_by_mask(":PWR", 0x01) or field_by_mask("PWR", 0x01) or field_by_mask(":PWR", 0x02) or field_by_mask("PWR", 0x02)
local col4_port = manager.machine.ioport.ports[":COL.4"] or manager.machine.ioport.ports["COL.4"]
local pwr_port = manager.machine.ioport.ports[":PWR"] or manager.machine.ioport.ports["PWR"]

if not quiet then
	for tag, port in pairs(manager.machine.ioport.ports) do
		if tag == ":COL.4" or tag == "COL.4" or tag == ":PWR" or tag == "PWR" then
			for name, field in pairs(port.fields) do
				emu.print_info(string.format(
					"input-field:port=%s key=%s name=%s mask=%02x type=%s def=%s",
					tag,
					tostring(name),
					field.name,
					field.mask,
					tostring(field.type),
					tostring(field.defvalue)))
			end
		end
	end
end

local function bp(addr, label)
	if not debug then
		return
	end
	debug:bpset(
		addr,
		"1",
		string.format(
			'logerror "probe:%s pc=%%08X lr=%%08X r0=%%08X r1=%%08X r2=%%08X r3=%%08X cur=%%02X irq=%%02X mask=%%02X rows=%%02X cols=%%02X q6=%%08X/%%08X/%%08X/%%08X\\n",pc,lr,r0,r1,r2,r3,b@0x100002,b@0x20009,b@0x2000b,b@0x20028,b@0x2002a,d@0x101750,d@0x101754,d@0x101758,d@0x10175c; g',
			label))
end

bp(0x0026a204, "send_a204")
bp(0x0026a354, "send_a354")
bp(0x0026a458, "recv_a458")
if debug then
	debug:bpset(0x00269bcc, "1", 'logerror "probe:copy_call pc=%08X lr=%08X sp=%08X r0=%08X r1=%08X r7=%08X r8=%08X r11=%08X fp=%08X q=%08X/%08X\\n",pc,lr,sp,r0,r1,r7,r8,r11,fp,d@0x101698,d@0x10169c; g')
	debug:bpset(0x002aca44, "1", 'logerror "probe:copy_arm_entry pc=%08X lr=%08X sp=%08X r0=%08X r1=%08X r11=%08X fp=%08X saved=%08X/%08X/%08X/%08X\\n",pc,lr,sp,r0,r1,r11,fp,d@sp,d@(sp+4),d@(sp+8),d@(sp+12); g')
	debug:bpset(0x002aca8c, "1", 'logerror "probe:copy_thumb_restore pc=%08X lr=%08X sp=%08X r0=%08X r1=%08X r11=%08X fp=%08X stack=%08X/%08X/%08X/%08X\\n",pc,lr,sp,r0,r1,r11,fp,d@sp,d@(sp+4),d@(sp+8),d@(sp+12); g')
	debug:bpset(0x00269bd0, "1", 'logerror "probe:copy_return pc=%08X lr=%08X sp=%08X r0=%08X r1=%08X r11=%08X fp=%08X q=%08X/%08X\\n",pc,lr,sp,r0,r1,r11,fp,d@0x101698,d@0x10169c; g')
	debug:bpset(0x0026a648, "1", 'logerror "probe:recv_store_next pc=%08X lr=%08X sp=%08X r0=%08X r1=%08X r2=%08X r3=%08X r11=%08X fp=%08X q=%08X/%08X node=%08X/%08X\\n",pc,lr,sp,r0,r1,r2,r3,r11,fp,d@0x101698,d@0x10169c,d@r0,d@(r0+4); g')
	debug:bpset(0x0026ff1a, "1", 'logerror "probe:event_raw_dequeued pc=%08X lr=%08X raw=%08X q6=%08X/%08X/%08X/%08X cur=%02X\\n",pc,lr,r0,d@0x101750,d@0x101754,d@0x101758,d@0x10175c,b@0x100002; g')
	debug:bpset(0x0026ff6e, "1", 'logerror "probe:event_raw_d5 pc=%08X lr=%08X raw=%08X q6=%08X/%08X/%08X/%08X cur=%02X\\n",pc,lr,r4,d@0x101750,d@0x101754,d@0x101758,d@0x10175c,b@0x100002; g')
	debug:bpset(0x0027006c, "1", 'logerror "probe:event_raw_15_return pc=%08X lr=%08X raw=%08X q6=%08X/%08X/%08X/%08X cur=%02X\\n",pc,lr,r4,d@0x101750,d@0x101754,d@0x101758,d@0x10175c,b@0x100002; g')
end
bp(0x00297fc4, "display_manager_entry")
bp(0x00298008, "display_recv")
bp(0x002b1e80, "display_queue_type1")
bp(0x002b1f24, "display_queue_type2")
bp(0x002b1f64, "display_queue_type3")

if debug then
	debug:bpset(0x0026a204, "r0==6", 'logerror "probe:send_task6_a204 pc=%08X lr=%08X r0=%08X r1=%08X msg=%08X/%08X/%08X/%08X bytes=%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\\n",pc,lr,r0,r1,d@r1,d@(r1+4),d@(r1+8),d@(r1+12),b@r1,b@(r1+1),b@(r1+2),b@(r1+3),b@(r1+4),b@(r1+5),b@(r1+6),b@(r1+7); g')
	debug:bpset(0x0026a354, "r0==6", 'logerror "probe:send_task6_a354 pc=%08X lr=%08X r0=%08X r1=%08X msg=%08X/%08X/%08X/%08X bytes=%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\\n",pc,lr,r0,r1,d@r1,d@(r1+4),d@(r1+8),d@(r1+12),b@r1,b@(r1+1),b@(r1+2),b@(r1+3),b@(r1+4),b@(r1+5),b@(r1+6),b@(r1+7); g')
	debug:bpset(0x0026a95c, "r0==6", 'logerror "probe:send_task6_a95c pc=%08X lr=%08X r0=%08X r1=%08X msg=%08X/%08X/%08X/%08X bytes=%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\\n",pc,lr,r0,r1,d@r1,d@(r1+4),d@(r1+8),d@(r1+12),b@r1,b@(r1+1),b@(r1+2),b@(r1+3),b@(r1+4),b@(r1+5),b@(r1+6),b@(r1+7); g')
	debug:bpset(0x0026aac0, "r0==6", 'logerror "probe:send_task6_aac0 pc=%08X lr=%08X r0=%08X r1=%08X msg=%08X/%08X/%08X/%08X bytes=%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\\n",pc,lr,r0,r1,d@r1,d@(r1+4),d@(r1+8),d@(r1+12),b@r1,b@(r1+1),b@(r1+2),b@(r1+3),b@(r1+4),b@(r1+5),b@(r1+6),b@(r1+7); g')
end

if not quiet then
	space:install_write_tap(0x200000, 0x5fffff, "noki3210_flash_write_probe", function(offset, data, mask)
		if flash_write_logs >= 80 then
			return
		end
		flash_write_logs = flash_write_logs + 1
		emu.print_info(string.format("flash-write:frame=%d addr=%08X pc=%08X data=%08X mask=%08X", frames, offset, cpu.state["PC"].value, data, mask))
	end)

	space:install_write_tap(0x600000, 0x9fffff, "noki3210_rom2_write_probe", function(offset, data, mask)
		if flash_write_logs >= 80 then
			return
		end
		flash_write_logs = flash_write_logs + 1
		emu.print_info(string.format("rom2-write:frame=%d addr=%08X pc=%08X data=%08X mask=%08X", frames, offset, cpu.state["PC"].value, data, mask))
	end)

	space:install_read_tap(0x20000, 0x200ff, "noki3210_input_col_read", function(offset, data, mask)
		if (offset & 0xff) ~= 0x2a then
			return
		end
		emu.print_info(string.format("input-col-read:frame=%d pc=%08X data=%02X mask=%02X rows=%02X irq=%02X", frames, cpu.state["PC"].value, data, mask, space:read_u8(0x20028), space:read_u8(0x20009)))
	end)

	space:install_write_tap(0x20000, 0x200ff, "noki3210_input_rowcol_write", function(offset, data, mask)
		local reg = offset & 0xff
		if reg ~= 0x28 and reg ~= 0x29 and reg ~= 0x2a and reg ~= 0x2b and reg ~= 0x69 and reg ~= 0x6b and reg ~= 0xa8 and reg ~= 0xaa then
			return
		end
		emu.print_info(string.format("input-rowcol-write:frame=%d addr=%08X pc=%08X data=%02X mask=%02X irq=%02X", frames, offset, cpu.state["PC"].value, data, mask, space:read_u8(0x20009)))
	end)
end

local function bus_byte(offset, data, mask)
	local address = offset & ~3
	if mask == 0xff000000 then
		return address, (data >> 24) & 0xff
	elseif mask == 0x00ff0000 then
		return address + 1, (data >> 16) & 0xff
	elseif mask == 0x0000ff00 then
		return address + 2, (data >> 8) & 0xff
	elseif mask == 0x000000ff then
		return address + 3, data & 0xff
	end
	return offset, data & 0xff
end

local function record_structural_mmio(offset, data, mask)
	local address, value = bus_byte(offset, data, mask)
	local reg = address & 0xff
		if reg == 0x01 and (value & 0x04) ~= 0 then
			structural.soft_resets = structural.soft_resets + 1
		elseif reg == 0x20 or reg == 0x24 then
			if reg == 0x20 then
				genio_signal = value
				structural.eeprom_signal_writes = structural.eeprom_signal_writes + 1
			else
				genio_direction = value
			end
			local next_sda = (genio_direction & 0x01) ~= 0 and (genio_signal & 0x01) or 1
			local next_scl = (genio_signal >> 3) & 1
			if eeprom_sda == 1 and next_sda == 0 and next_scl == 1 then
				structural.eeprom_starts = structural.eeprom_starts + 1
			end
			eeprom_sda = next_sda
			eeprom_scl = next_scl
		elseif reg == 0x2d then
			gensio_control = value
			structural.gensio_controls[gensio_control] = true
			if (gensio_control & 0x04) ~= 0 then
				ccont_command_phase = true
			end
		elseif reg == 0x2c and (gensio_control & 0x04) ~= 0 then
			structural.ccont_bytes = structural.ccont_bytes + 1
			if ccont_command_phase then
				structural.ccont_commands[value] = true
			end
			ccont_command_phase = not ccont_command_phase
		end
end

space:install_write_tap(0x20000, 0x200ff, "noki3210_input_lcd", function(offset, data, mask)
	record_structural_mmio(offset, data, mask)
	local address, value = bus_byte(offset, data, mask)
	local reg = address & 0xff
	data = value
	mask = 0xff
	if reg == 0x2e then
		lcd_data_writes = lcd_data_writes + 1
		if (data & 0xff) ~= 0 then
			nonzero_lcd_data_writes = nonzero_lcd_data_writes + 1
			if not quiet then
				emu.print_info(string.format("lcd-data-nonzero:frame=%d pc=%08X data=%02X count=%d", frames, cpu.state["PC"].value, data & 0xff, nonzero_lcd_data_writes))
			end
		end

		local old_x = lcd_mirror_x
		local old_y = lcd_mirror_y
		lcd_mirror_vram[(lcd_mirror_y * 84) + lcd_mirror_x] = data & 0xff
		if (lcd_mirror_mode & 0x02) ~= 0 then
			lcd_mirror_y = lcd_mirror_y + 1
			if lcd_mirror_y > 5 then
				lcd_mirror_y = 0
				lcd_mirror_x = (lcd_mirror_x + 1) % 84
			end
		else
			lcd_mirror_x = lcd_mirror_x + 1
			if lcd_mirror_x > 83 then
				lcd_mirror_x = 0
				lcd_mirror_y = (lcd_mirror_y + 1) % 6
			end
		end

		if old_x == 83 and old_y == 5 and lcd_mirror_x == 0 and lcd_mirror_y == 0 then
			local zero = 0
			local ff = 0
			local other = 0
			for i = 0, (84 * 6) - 1 do
				local b = lcd_mirror_vram[i]
				if b == 0 then
					zero = zero + 1
				elseif b == 0xff then
					ff = ff + 1
				else
					other = other + 1
				end
			end

			lcd_mirror_dumps = lcd_mirror_dumps + 1
			lcd_mirror_pending = { seq = lcd_mirror_dumps, zero = zero, ff = ff, other = other }
			if other > 0 and not dumped_nonblank_lcd_screen and dump_screen then
				dumped_nonblank_lcd_screen = true
			end
		end
	elseif reg == 0x6e then
		lcd_cmd_writes = lcd_cmd_writes + 1
		local cmd = data & 0xff
		if (lcd_mirror_mode & 0x01) ~= 0 then
			if (cmd & 0x20) ~= 0 then
				lcd_mirror_mode = cmd & 0x07
			end
		else
			if (cmd & 0x80) ~= 0 then
				lcd_mirror_x = (cmd & 0x7f) % 84
			elseif (cmd & 0x40) ~= 0 then
				lcd_mirror_y = cmd & 0x07
			elseif (cmd & 0x20) ~= 0 then
				lcd_mirror_mode = cmd & 0x07
			elseif (cmd & 0x08) ~= 0 then
				lcd_mirror_control = ((cmd & 0x04) >> 1) | (cmd & 0x01)
			end
		end
	end
end)

space:install_read_tap(0x2006c, 0x2006f, "noki3210_summary_ccont_read", function(offset, data, mask)
	local address = bus_byte(offset, data, mask)
	if (address & 0xff) == 0x6c then
		structural.ccont_reads = structural.ccont_reads + 1
	end
end)

local function hex_set(values, width)
	local result = {}
	for value in pairs(values) do
		result[#result + 1] = string.format("%0" .. width .. "X", value)
	end
	table.sort(result)
	return table.concat(result, ",")
end

local function write_boot_summary()
	if not boot_summary_path then
		return
	end
	local summary = {
		"schema=1",
		string.format("frames=%d", frames),
		string.format("lcd_data_writes=%d", lcd_data_writes),
		string.format("lcd_command_writes=%d", lcd_cmd_writes),
		string.format("lcd_nonzero_writes=%d", nonzero_lcd_data_writes),
		string.format("lcd_full_dumps=%d", lcd_mirror_dumps),
		string.format("soft_resets=%d", structural.soft_resets),
		string.format("irq_seen=%02X", structural.irq_seen),
		string.format("fiq_seen=%02X", structural.fiq_seen),
		string.format("gensio_controls=%s", hex_set(structural.gensio_controls, 2)),
		string.format("ccont_bytes=%d", structural.ccont_bytes),
		string.format("ccont_reads=%d", structural.ccont_reads),
		string.format("ccont_commands=%s", hex_set(structural.ccont_commands, 2)),
		string.format("eeprom_starts=%d", structural.eeprom_starts),
		string.format("eeprom_signal_writes=%d", structural.eeprom_signal_writes),
		string.format("startup_modes=%s", hex_set(structural.startup_modes, 4)),
		string.format("final_current_task=%02X", structural.final_current_task),
		string.format("final_startup_mode=%04X", structural.final_startup_mode),
		string.format("final_startup_event=%04X", structural.final_startup_event),
		string.format("final_startup_flags=%02X", structural.final_startup_flags),
		string.format("final_contact_status=%04X", structural.final_contact_status),
		string.format("final_no_sim=%02X", structural.final_no_sim),
		string.format("final_sim_enable=%02X", structural.final_sim_enable),
	}
	local temporary = boot_summary_path .. ".tmp"
	local f = io.open(temporary, "w")
	if f then
		f:write(table.concat(summary, "\n"), "\n")
		f:close()
		os.rename(temporary, boot_summary_path)
	end
end

local function press(name, field)
	if not field then
		return
	end
	local col4_before = col4_port and (col4_port:read() & 0xff) or -1
	local pwr_before = pwr_port and (pwr_port:read() & 0xff) or -1
	local ok, err = pcall(function()
		field:set_value(0)
	end)
	if not ok then
		emu.print_info(string.format("input-press-error:frame=%d name=%s err=%s", frames, name, tostring(err)))
		return
	end
	active_fields[name] = field
	local col4_after = col4_port and (col4_port:read() & 0xff) or -1
	local pwr_after = pwr_port and (pwr_port:read() & 0xff) or -1
	emu.print_info(string.format("input-press:frame=%d name=%s col4=%02X->%02X pwr=%02X->%02X", frames, name, col4_before, col4_after, pwr_before, pwr_after))
end

local function release(name)
	local field = active_fields[name]
	if not field then
		return
	end
	local col4_before = col4_port and (col4_port:read() & 0xff) or -1
	local pwr_before = pwr_port and (pwr_port:read() & 0xff) or -1
	local ok, err = pcall(function()
		field:clear_value()
	end)
	if not ok then
		emu.print_info(string.format("input-release-error:frame=%d name=%s err=%s", frames, name, tostring(err)))
	end
	active_fields[name] = nil
	local col4_after = col4_port and (col4_port:read() & 0xff) or -1
	local pwr_after = pwr_port and (pwr_port:read() & 0xff) or -1
	emu.print_info(string.format("input-release:frame=%d name=%s col4=%02X->%02X pwr=%02X->%02X", frames, name, col4_before, col4_after, pwr_before, pwr_after))
end

local function dump_ram(label)
	local dumps = {
		{ 0x100000, 0x2000, string.format("noki3210_ram_100000_%s.bin", label) },
		{ 0x109000, 0x800, string.format("noki3210_ram_109000_%s.bin", label) },
		{ 0x110000, 0x1000, string.format("noki3210_ram_110000_%s.bin", label) },
		{ 0x111000, 0x1000, string.format("noki3210_ram_111000_%s.bin", label) },
		{ 0x112000, 0x3000, string.format("noki3210_ram_112000_%s.bin", label) },
		{ 0x114400, 0x400, string.format("noki3210_ram_114400_%s.bin", label) },
		{ 0x116800, 0x200, string.format("noki3210_ram_116800_%s.bin", label) },
		{ 0x11f000, 0x1000, string.format("noki3210_ram_11f000_%s.bin", label) },
	}

	for _, dump in ipairs(dumps) do
		local addr, size, name = dump[1], dump[2], dump[3]
		local path = string.format("%s/%s", output_dir, name)
		local f = io.open(path, "wb")
		if f then
			for offset = 0, size - 1 do
				f:write(string.char(space:read_u8(addr + offset)))
			end
			f:close()
			emu.print_info(string.format("ram-dump:file=%s addr=%06X size=%d", path, addr, size))
		end
	end
end

dump_screen = function(label)
	if not screen then
		return
	end

	local hist = {}
	for y = 0, 47 do
		for x = 0, 83 do
			local pixel = screen:pixel(x, y)
			hist[pixel] = (hist[pixel] or 0) + 1
		end
	end

	local parts = {}
	for color, count in pairs(hist) do
		parts[#parts + 1] = string.format("%08X=%d", color, count)
	end
	table.sort(parts)

	local filename = string.format("%s/noki3210_%s.png", output_dir, label)
	local err = screen:snapshot(filename)
	emu.print_info(string.format(
		"screen-dump:file=%s frame=%d err=%s hist=%s",
		filename,
		frames,
		tostring(err),
		table.concat(parts, ",")))
end

local function write_lcd_mirror()
	if not lcd_mirror_pending then
		return
	end
	local pending = lcd_mirror_pending
	lcd_mirror_pending = nil
	local filename = string.format("%s/noki3210_lcdmirror_%04d_f%03d_z%03d_ff%03d_o%03d.pgm",
		output_dir, pending.seq, frames, pending.zero, pending.ff, pending.other)
	local f = io.open(filename, "wb")
	if not f then
		return
	end
	f:write("P5\n84 48\n255\n")
	for y = 0, 47 do
		local row = y >> 3
		local bit = y & 7
		for x = 0, 83 do
			local v = lcd_mirror_vram[(row * 84) + x]
			local on = (v >> bit) & 1
			if (lcd_mirror_control & 0x01) ~= 0 then
				on = 1 - on
			end
			f:write(string.char(on ~= 0 and 0 or 255))
		end
	end
	f:close()
end

emu.add_machine_frame_notifier(function()
	frames = frames + 1
	write_lcd_mirror()
	structural.irq_seen = structural.irq_seen | space:read_u8(0x20009)
	structural.fiq_seen = structural.fiq_seen | space:read_u8(0x20008)
	structural.final_current_task = space:read_u8(0x100002)
	structural.final_startup_mode = space:read_u16(0x1123f0)
	structural.final_startup_event = space:read_u16(0x1123ee)
	structural.final_startup_flags = space:read_u8(0x112399)
	structural.final_contact_status = space:read_u16(0x11fed0)
	structural.final_no_sim = space:read_u8(0x111c64)
	structural.final_sim_enable = space:read_u8(0x111c79)
	structural.startup_modes[structural.final_startup_mode] = true
	if (frames % 30) == 0 then
		write_boot_summary()
	end

	if exercise_input and frames == 18 then
		press("enter", enter_field)
	elseif exercise_input and frames == 24 then
		release("enter")
	elseif exercise_input and frames == 30 then
		press("enter", enter_field)
	elseif exercise_input and frames == 36 then
		release("enter")
	elseif exercise_input and frames == 42 then
		press("power", power_field)
	elseif exercise_input and frames == 48 then
		release("power")
	elseif exercise_input and frames == 54 then
		press("enter", enter_field)
	elseif exercise_input and frames == 60 then
		release("enter")
	elseif exercise_input and frames == 66 then
		press("enter", enter_field)
	elseif exercise_input and frames == 72 then
		release("enter")
	end

	if not quiet and (frames % 30) == 0 then
		emu.print_info(string.format(
			"input-exerciser:frame=%d cur=%02X irq=%02X mask=%02X rows=%02X cols=%02X lcd=%d/%d nonzero=%d q6=%08X/%08X/%08X/%08X",
			frames,
			space:read_u8(0x100002),
			space:read_u8(0x20009),
			space:read_u8(0x2000b),
			space:read_u8(0x20028),
			space:read_u8(0x2002a),
			lcd_data_writes,
			lcd_cmd_writes,
			nonzero_lcd_data_writes,
			space:read_u32(0x101750),
			space:read_u32(0x101754),
			space:read_u32(0x101758),
			space:read_u32(0x10175c)))
	end

	if not quiet and ((frames % 30) == 0 or frames == 24 or frames == 36 or frames == 48 or frames == 60 or frames == 72 or frames == 210 or frames == 300) and not dumped_frames[frames] then
		dumped_frames[frames] = true
		if frames == 210 or frames == 300 then
			dump_ram(string.format("frame%03d", frames))
		end
		dump_screen(string.format("frame%03d", frames))
	end
end)

emu.add_machine_stop_notifier(function()
	for name in pairs(active_fields) do
		release(name)
	end
	if not quiet then
		dump_ram("stop")
		dump_screen("stop")
	end
	emu.print_info(string.format("input-exerciser:stop lcd=%d/%d nonzero=%d", lcd_data_writes, lcd_cmd_writes, nonzero_lcd_data_writes))
	write_boot_summary()
end)

emu.print_info("noki3210 input exerciser: installed")
if debug then
	debug:go()
end
