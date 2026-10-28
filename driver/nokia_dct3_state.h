// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_DCT3_STATE_H
#define MAME_NOKIA_NOKIA_DCT3_STATE_H

#include "nokia_dct3_config.h"
#include "nokia_dct3_trace_config.h"
#include "cpu/arm7/arm7.h"
#include "machine/i2cmem.h"
#include "machine/intelfsh.h"
#include "sound/beep.h"
#include "video/pcd8544.h"
#include "video/sed1520.h"
#include "nokia_ccont.h"
#include "nokia_dsp_c54x.h"
#include "nokia_dspif.h"
#include "nokia_gsm_call_adapter.h"
#include "nokia_gsm_network.h"
#include "nokia_gsm_session.h"
#include "nokia_lapdm_link.h"
#include "nokia_mbus.h"
#include "nokia_mbus_terminal.h"
#include "nokia_pup.h"
#include "nokia_sim_card.h"
#include "nokia_simi.h"
#include "nokia_uif.h"
#include <unordered_map>
#include <unordered_set>

namespace nokia_dct3 {

class nokia_dct3_state : public driver_device
{
public:
	nokia_dct3_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_flash(*this, "flash"),
		m_eeprom(*this, "eeprom"),
		m_ccont(*this, "ccont"),
		m_cobba(*this, "cobba"),
		m_gensio(*this, "gensio"),
		m_mad2(*this, "mad2"),
		m_mad2_pcm(*this, "mad2_pcm"),
		m_kbgpio(*this, "kbgpio"),
		m_uif(*this, "uif"),
		m_mbus(*this, "mbus"),
		m_mbus_terminal(*this, "mbus_terminal"),
		m_pup(*this, "pup"),
		m_dspif(*this, "dspif"),
		m_dsp_c54x(*this, "dsp_c54x"),
		m_dsp_hle(*this, "dsp_hle"),
		m_external_service_peer(*this, "external_service_peer"),
		m_gsm_call_adapter(*this, "gsm_call_adapter"),
		m_gsm_network(*this, "gsm_network"),
		m_gsm_session(*this, "gsm_session"),
		m_lapdm_link(*this, "lapdm_link"),
		m_radio_peer(*this, "radio_peer"),
		m_simi(*this, "simi"),
		m_sim_card(*this, "sim_card"),
		m_lcd(*this, "lcd"),
		m_sed_lcd(*this, "sed_lcd"),
		m_buzzer(*this, "buzzer"),
		m_dsp_tone1(*this, "dsp_tone1"),
		m_dsp_tone2(*this, "dsp_tone2"),
		m_vibration(*this, "vibration"),
		m_hw_config(*this, "HWCFG"),
		m_diag_config(*this, "DIAGCFG"),
		m_research_external_status(*this, "RESEARCH_EXT_STATUS"),
		m_research_adc0(*this, "RESEARCH_ADC0"),
		m_research_adc2(*this, "RESEARCH_ADC2"),
		m_network_config(*this, "NETCFG"),
		m_sms_config(*this, "SMSCFG"),
		m_ussd_config(*this, "USSDCFG"),
		m_ems_config(*this, "EMSCFG"),
		m_smart_message_config(*this, "SMARTCFG"),
		m_sim_toolkit_config(*this, "SATCFG"),
		m_cell_config(*this, "CELLCFG"),
		m_assignment_config(*this, "ASSIGNCFG"),
		m_page_config(*this, "PAGECFG"),
		m_authentication_config(*this, "AUTHCFG"),
		m_cipher_config(*this, "CIPHERCFG"),
		m_mobility_config(*this, "MOBILITYCFG"),
		m_neighbour_config(*this, "NEIGHBORCFG"),
		m_neighbour_fault_config(*this, "NEIGHBORFAULT"),
		m_outgoing_call_config(*this, "CALLCFG"),
		m_outgoing_call_delay_config(*this, "CALLDELAY"),
		m_outgoing_call_host_config(*this, "CALLHOST"),
		m_handover_config(*this, "HANDOVERCFG"),
		m_sim_removed(*this, "SIM_REMOVED")
	{ }

	void noki3330(machine_config &config);
	void noki3350(machine_config &config);
	void noki8855(machine_config &config);
	void nsm4stage(machine_config &config);
	void nhm9stage(machine_config &config);
	void noki3410(machine_config &config);
	void noki2100(machine_config &config);
	void nam2c3(machine_config &config);
	void configure_native_dsp_attachments(nokia_dsp_c54x_device &backend);
	void configure_staged_dsp_attachments(nokia_dsp_staged_device &backend);
	void noki5110(machine_config &config);
	void noki5110i(machine_config &config);
	void nse2r4t(machine_config &config);
	void nse2r4v(machine_config &config);
	void noki5190(machine_config &config);
	void noki6190(machine_config &config);
	void nsb3r4t(machine_config &config);
	void nsb1r4t(machine_config &config);
	void nsk1r4t(machine_config &config);
	void noki6130(machine_config &config);
	void nsk3r4t(machine_config &config);
	void noki6110(machine_config &config);
	void nsm1r4t(machine_config &config);
	void nse6stage(machine_config &config);
	void nse6r4t(machine_config &config);
	void noki7110(machine_config &config);
	void nse5r4t(machine_config &config);
	void noki6210(machine_config &config);
	void npe3stage(machine_config &config);
	void npe3hle(machine_config &config);
	void npe3mbus(machine_config &config);
	void nmp5stage(machine_config &config);
	void nmp5hle(machine_config &config);
	void noki6250(machine_config &config);
	void nhm3stage(machine_config &config);
	void nhm3hle(machine_config &config);
	void dct3_base(machine_config &config);
	void dct3_32mbit_flash_base(machine_config &config);
	void noki3310(machine_config &config);
	void noki3610(machine_config &config);
	void noki3210(machine_config &config);
	void noki5210(machine_config &config);
	void noki8xxx(machine_config &config);
	void noki8290(machine_config &config);
	void nsb7stage(machine_config &config);
	void nsb7hle(machine_config &config);
	void noki8850(machine_config &config);
	void nsm2stage(machine_config &config);
	void nsm2hle(machine_config &config);
	void nsm3dr6(machine_config &config);
	void nsm3dhle(machine_config &config);
	void nsb6stage(machine_config &config);
	void nsb6hle(machine_config &config);
	void noki8210(machine_config &config);
	void nsm3stage(machine_config &config);
	void nsm3hle(machine_config &config);

	DECLARE_INPUT_CHANGED_MEMBER(key_irq);
	DECLARE_INPUT_CHANGED_MEMBER(power_key_irq);
	DECLARE_INPUT_CHANGED_MEMBER(charger_irq);
	DECLARE_INPUT_CHANGED_MEMBER(mbus_rx_byte);
	DECLARE_INPUT_CHANGED_MEMBER(sms_config_changed);
	DECLARE_INPUT_CHANGED_MEMBER(sim_removed_changed);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;
private:
	void post_load();
	void apply_product_config(nokia_product_config const &product);
	void apply_sms_config();
	u8 nse5_roller_gpio_r(offs_t bank);


	uint8_t mad2_io_r(offs_t offset);
	void mad2_io_w(offs_t offset, uint8_t data);
	uint8_t mad2_register_r(offs_t offset);
	uint8_t mad2_register_peek(offs_t offset);
	void mad2_register_w(offs_t offset, uint8_t data);
	void trace_mad2_read(offs_t offset, uint8_t data);
	void trace_mad2_write(offs_t offset, uint8_t data, uint8_t old_data);
	uint8_t mad2_dspif_r(offs_t offset);
	void mad2_dspif_w(offs_t offset, uint8_t data);
	uint8_t mad2_mcuif_r(offs_t offset);
	void mad2_mcuif_w(offs_t offset, uint8_t data);

	TIMER_CALLBACK_MEMBER(timer_watchdog);

	uint16_t ram_r(offs_t offset, uint16_t mem_mask = ~0);
	void ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t dsp_ram_r(offs_t offset);
	void dsp_ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t flash_r(offs_t offset, uint16_t mem_mask = ~0);
#if DCT3_RESEARCH_TRACES
	void flash_firmware_traces(u32 pc, u32 addr);
	u8 trace_running_task() const;
#endif
	void flash_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint32_t rom2_mirror_r(offs_t offset, uint32_t mem_mask = ~0);
	void rom2_mirror_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

	void dct3_map(address_map &map) ATTR_COLD;
	void dct3_nhm9_map(address_map &map) ATTR_COLD;
	void dct3_nsb1_map(address_map &map) ATTR_COLD;
	void dct3_nse2_map(address_map &map) ATTR_COLD;
	void dct3_nse3_map(address_map &map) ATTR_COLD;
	void dct3_nsm1_research_map(address_map &map) ATTR_COLD;
	void dct3_nse6_research_map(address_map &map) ATTR_COLD;

	void trace_interrupt_register(char operation, offs_t offset, uint8_t data);
	void mad2_fiq_w(int state);
	void mad2_irq_w(int state);
	void mad2_sleep_w(int state);
	void mad2_irq_ack_w(u16 mask);
	void kbgpio_irq_w(int state);
	template <unsigned Column> u8 keypad_matrix_r();
	void pup_buzzer_clock_w(u32 frequency);
	void pup_buzzer_enable_w(int state);
	void pup_vibrator_w(int state);
	void mad2_reset_w(int state);
	TIMER_CALLBACK_MEMBER(deferred_mad2_reset);
	void ccont_irq_w(int state);
	void ccont_power_w(int state);
	void reset_digital_baseband(bool retain_sram = false);
	void reset_board_state(bool clear_sram);
	void sim_irq_w(int state);
	void sim_detect_w(int state);
	void mbus_fiq2_w(int state);
	void mbus_fiq3_w(int state);
	void mbus_tx_w(u8 data);
	void dsp_fiq0_w(int state);
	void dsp_fiq1_w(int state);
	void dsp_service_irq_w(int state);
	void dsp_tx_commit_w(int state);
	void dsp_service_pending_w(int state);
	void dsp_doorbell_w(int state);
	void dsp_reset_w(int state);
	void dsp_shared_002_write_w(int state);
	void dsp_shared_006_write_w(int state);
	void dsp_shared_0fe_read_w(int state);
	void dsp_shared_0fe_write_w(int state);
	void dsp_shared_100_read_w(int state);
	void dsp_shared_100_write_w(int state);
	void dsp_tone_update_w(int state);
	// Observation-only helpers implemented in nokia_dct3_trace.inc.
#if DCT3_RESEARCH_TRACES
	uint16_t fw_word(offs_t address) const;
	uint8_t fw_byte(offs_t address) const;
	uint32_t fw_dword(offs_t address) const;
	void trace_dsp_audio_shadow_write(
			offs_t address, uint16_t old_data, uint16_t data);
	void trace_radio_pending_primitive_write(
			offs_t address, uint16_t old_data, uint16_t data);
#endif
	required_device<cpu_device> m_maincpu;
	required_device<intelfsh16_device> m_flash;
	optional_device<i2cmem_device> m_eeprom;
	required_device<nokia_ccont_device> m_ccont;
	required_device<nokia_cobba_device> m_cobba;
	required_device<nokia_gensio_device> m_gensio;
	required_device<nokia_mad2_device> m_mad2;
	required_device<nokia_mad2_pcm_device> m_mad2_pcm;
	required_device<nokia_kbgpio_device> m_kbgpio;
	required_device<nokia_uif_device> m_uif;
	required_device<nokia_mbus_device> m_mbus;
	required_device<nokia_mbus_terminal_device> m_mbus_terminal;
	required_device<nokia_pup_device> m_pup;
	required_device<nokia_dspif_device> m_dspif;
	nokia_dsp_backend_interface *m_dsp_backend = nullptr;
	optional_device<nokia_dsp_c54x_device> m_dsp_c54x;
	optional_device<nokia_dsp_hle_device> m_dsp_hle;
	required_device<nokia_external_service_peer_device> m_external_service_peer;
	required_device<nokia_gsm_call_adapter_device> m_gsm_call_adapter;
	required_device<nokia_gsm_network_device> m_gsm_network;
	required_device<nokia_gsm_session_device> m_gsm_session;
	required_device<nokia_lapdm_link_device> m_lapdm_link;
	required_device<nokia_radio_peer_device> m_radio_peer;
	required_device<nokia_simi_device> m_simi;
	required_device<nokia_sim_card_device> m_sim_card;
	optional_device<pcd8544_device> m_lcd;
	optional_device<sed1565_device> m_sed_lcd;
	required_device<beep_device> m_buzzer;
	required_device<beep_device> m_dsp_tone1;
	required_device<beep_device> m_dsp_tone2;
	output_finder<> m_vibration;
	optional_ioport m_hw_config;
	optional_ioport m_diag_config;
	optional_ioport m_research_external_status;
	optional_ioport m_research_adc0;
	optional_ioport m_research_adc2;
	optional_ioport m_network_config;
	optional_ioport m_sms_config;
	optional_ioport m_ussd_config;
	optional_ioport m_ems_config;
	optional_ioport m_smart_message_config;
	optional_ioport m_sim_toolkit_config;
	optional_ioport m_cell_config;
	optional_ioport m_assignment_config;
	optional_ioport m_page_config;
	optional_ioport m_authentication_config;
	optional_ioport m_cipher_config;
	optional_ioport m_mobility_config;
	optional_ioport m_neighbour_config;
	optional_ioport m_neighbour_fault_config;
	optional_ioport m_outgoing_call_config;
	optional_ioport m_outgoing_call_delay_config;
	optional_ioport m_outgoing_call_host_config;
	optional_ioport m_handover_config;
	optional_ioport m_sim_removed;

	std::unique_ptr<uint16_t[]>   m_ram;

	nokia_product_config m_product = PRODUCT_DEFAULT;
	bool          m_ccont_irq_state;
	bool          m_baseband_powered = true;
	bool          m_keypad_port_refresh = false;

	emu_timer * m_timer_watchdog;

	uint8_t       m_mad2_regs[0x100];
	bool          m_mad2_trace_read[0x100] = {false};
	bool          m_mad2_trace_write[0x100] = {false};
	bool          m_dspif_trace_read[4] = {false};
	bool          m_dspif_trace_write[4] = {false};
	std::unordered_map<uint64_t, uint16_t> m_dsp_shared_trace_reads;
	std::unordered_set<uint64_t> m_flash_persistent_trace_reads;
	bool          m_mcuif_trace_read[4] = {false};
	bool          m_mcuif_trace_write[4] = {false};
	uint8_t       m_mcuif_regs[4] = {0};
	unsigned      m_gensio_trace_count = 0;
	unsigned      m_display_io_trace_count = 0;
	unsigned      m_mad2_timer_trace_count = 0;
	unsigned      m_mad2_interrupt_trace_count = 0;
	unsigned      m_mad2_clock_trace_count = 0;
	unsigned      m_mbus_trace_count = 0;

	bool m_trace_enabled = false;
};

INPUT_PORTS_EXTERN(dct3_network_config);
INPUT_PORTS_EXTERN(noki5110);
INPUT_PORTS_EXTERN(noki5210);

} // namespace nokia_dct3

#endif // MAME_NOKIA_NOKIA_DCT3_STATE_H
