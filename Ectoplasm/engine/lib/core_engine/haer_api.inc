// Ectoplasm (Haer, 2026-11-20). GPL-3.0-only, like the firmware it extends.
//
// On the hardware, many controls are "shifted": hold TAP or MODE and turn a
// knob. Ectoplasm gives each of them its own knob and CV. These functions
// repeat the firmware's own shifted-knob code (lib/ectocore.h, input_handling)
// so the sound is identical; they only skip the LED-wheel feedback.
// All `val` arguments use the firmware's knob scale, 0..1023.
// Included after api.inc, so prepare.py moves every global they touch into the
// per-instance CoreEngine just like the original firmware code.

// hold TAP + BREAK: volume up to 1/2, then saturation, then fuzz
void core_haer_drive(CoreEngine *e,int val) {
    CoreEngine *old=core_enter(e);
    uint8_t saved=gpio_btn_taptempo_val;gpio_btn_taptempo_val=0;
    break_set(val,false,false);
    gpio_btn_taptempo_val=saved;
    core_leave(old);
}

// hold TAP + AMEN: DJ filter, low-pass left of centre, high-pass right
void core_haer_filter(CoreEngine *e,int val) {
    CoreEngine *old=core_enter(e);
    const uint16_t val_mid=60;
    if(val<512-val_mid) {
        global_filter_index=val*(resonantfilter_fc_max)/(512-val_mid);global_filter_lphp=0;
    } else if(val>512+val_mid) {
        global_filter_index=(val-(512+val_mid))*(resonantfilter_fc_max)/(512-val_mid);global_filter_lphp=1;
    } else {
        global_filter_index=resonantfilter_fc_max;global_filter_lphp=0;
    }
    for(uint8_t channel=0;channel<2;channel++) {
        ResonantFilter_setFilterType(resFilter[channel],global_filter_lphp);
        ResonantFilter_setFc(resFilter[channel],global_filter_index);
    }
    core_leave(old);
}

// hold TAP + SAMPLE: slice gating ("Tighten"), off below 1/8
void core_haer_gate(CoreEngine *e,int val) {
    CoreEngine *old=core_enter(e);
    if(val<128) {
        if(sf->fx_active[FX_TIGHTEN])toggle_fx(FX_TIGHTEN);
    } else {
        if(!sf->fx_active[FX_TIGHTEN])toggle_fx(FX_TIGHTEN);
        sf->fx_param[FX_TIGHTEN][0]=linlin(val,128,1024,0,200);
        Gate_set_amount(audio_gate,45+200-sf->fx_param[FX_TIGHTEN][0]);
    }
    core_leave(old);
}

// hold TAP + GRIMOIRE: probability of a retrigger (stutter) at phrase ends
void core_haer_retrig(CoreEngine *e,int val) {
    CoreEngine *old=core_enter(e);planned_retrig_probability=val*100/1024;core_leave(old);
}

// hold TAP + JUMP: pitch down left of centre, up right of centre
void core_haer_pitch(CoreEngine *e,int val) {
    CoreEngine *old=core_enter(e);
    if(val<512-24)sf->pitch_val_index=PITCH_VAL_MID-(uint8_t)((512-24-val)*PITCH_VAL_MID/(512-24));
    else if(val>512+24)sf->pitch_val_index=PITCH_VAL_MID+(uint8_t)((val-(512+24))*(PITCH_VAL_MAX-PITCH_VAL_MID)/(512-24));
    else sf->pitch_val_index=PITCH_VAL_MID;
    core_leave(old);
}

// hold MODE layer: 0 amiga, 1 saturate, 2 smear, 3 jitter, 4 chaos
void core_haer_mode_layer(CoreEngine *e,int which,int val) {
    CoreEngine *old=core_enter(e);
    switch(which) {
        case 0: mode_amiga_index=val*37/1024; break;
        case 1: mode_digital_saturation=val*100/1024; break;
        case 2: mode_digital_smear=val*100/1024; break;
        case 3: mode_digital_jitter=val*100/1024; break;
        case 4: mode_chaos_trembler=val*100/1024; break;
    }
    core_leave(old);
}

// Tempo knob/CV; ignored while an external clock is driving the module.
void core_haer_tempo(CoreEngine *e,int bpm) {
    CoreEngine *old=core_enter(e);
    if(bpm<30)bpm=30;if(bpm>300)bpm=300;
    if(clock_input_absent&&sf->bpm_tempo!=bpm)sf->bpm_tempo=bpm;
    core_leave(old);
}

// Bank knob/CV: index into the banks that hold samples (0..count-1).
void core_haer_bank(CoreEngine *e,int index) {
    CoreEngine *old=core_enter(e);
    if(banks_with_samples_num>0) {
        if(index<0)index=0;if(index>=banks_with_samples_num)index=banks_with_samples_num-1;
        haer_bank_request=banks_with_samples[index];
    }
    core_leave(old);
}
int core_haer_bank_count(CoreEngine *e) {
    CoreEngine *old=core_enter(e);int n=banks_with_samples_num;core_leave(old);return n;
}

// PLAY: the hardware's TAP + MULT start/stop.
void core_haer_play_toggle(CoreEngine *e) {
    CoreEngine *old=core_enter(e);
    if(!playback_stopped&&!do_stop_playback) {
        if(!button_mute)trigger_button_mute=true;
        do_stop_playback=true;
    } else if(playback_stopped&&!do_restart_playback) {
        timer_reset();
    }
    TapTempo_reset(taptempo);
    core_leave(old);
}

// RESET: with a clock, the next clock pulse becomes beat 1 (send RESET with or
// just before that clock pulse); without a clock, restart from the top now.
void core_haer_reset(CoreEngine *e) {
    CoreEngine *old=core_enter(e);
    if(!clock_input_absent) {
        clock_in_beat_total=-1;
        if(metadata_selection_valid(sel_bank_cur,sel_sample_cur)&&banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->splice_variable)
            variable_splice_reset=VARIABLE_SPLICE_RESET_CLOCK;
    } else if(!playback_stopped) {
        timer_reset();
    }
    core_leave(old);
}
