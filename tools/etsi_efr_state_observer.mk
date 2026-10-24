# The first makefile is the acquired reference's own makefile.gcc.
decoder_observed: $(DOBJ) efr_state_observer.o
	$(CC) $(CFLAGS) -o $@ $(DOBJ) efr_state_observer.o \
		-Wl,--wrap=Decoder_12k2,--wrap=D_plsf_5,--wrap=d_gain_pitch,--wrap=d_gain_code,--wrap=Syn_filt,--wrap=Pred_lt_6

efr_state_observer.o: $(OBSERVER_SOURCE)
	$(CC) $(CFLAGS) -c $(OBSERVER_SOURCE) -o $@

# The standalone homing helper depends on main's synthesis buffer; neither
# belongs to this classifier-only entry point.
sid_observed: $(filter-out decoder.o d_homing.o,$(DOBJ)) efr_sid_observer.o
	$(CC) $(CFLAGS) -o $@ $(filter-out decoder.o d_homing.o,$(DOBJ)) efr_sid_observer.o

efr_sid_observer.o: $(SID_OBSERVER_SOURCE)
	$(CC) $(CFLAGS) -c $(SID_OBSERVER_SOURCE) -o $@

decoder_dtx_observed: $(DOBJ) efr_dtx_observer.o
	$(CC) $(CFLAGS) -o $@ $(DOBJ) efr_dtx_observer.o -Wl,--wrap=rx_dtx

efr_dtx_observer.o: $(DTX_OBSERVER_SOURCE)
	$(CC) $(CFLAGS) -c $(DTX_OBSERVER_SOURCE) -o $@
