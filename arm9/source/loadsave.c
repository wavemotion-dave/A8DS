/*
 * loadsave.c contains routines for saving state and loading state
 * 
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)

 * Copying and distribution of this emulator, its source code and associated 
 * readme files, with or without modification, are permitted in any medium without 
 * royalty provided this full copyright notice (including the Atari800 one below) 
 * is used and wavemotion-dave, alekmaul (original port), Atari800 team (for the
 * original source) and Avery Lee (Altirra OS) are credited and thanked profusely.
 * 
 * The A8DS emulator is offered as-is, without any warranty.
 * 
 * Since much of the original codebase came from the Atari800 project, and since
 * that project is released under the GPL V2, this program and source must also
 * be distributed using that same licensing model. See COPYING for the full license.
 */
#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "main.h"
#include "a8ds.h"

#include "atari.h"
#include "antic.h"
#include "cartridge.h"
#include "input.h"
#include "esc.h"
#include "rtime.h"
#include "cpu.h"
#include "gtia.h"
#include "pia.h"
#include "sio.h"
#include "pokey.h"
#include "pokeysnd.h"
#include "cycle_map.h"
#include "memory.h"
#include "config.h"
#include "loadsave.h"

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

#define SAVE_FILE_REV   0x0006

char save_filename[300+4];

u32 XE_MemUsed(void)
{
    u32 idx=(1024*1024)-1;
    
    while (idx > 0)
    {
        if (xe_mem_buffer[idx] != 0x00) return (idx+1);
        idx--;
    }    
    
    return 0;
}


// -------------------------------------------------------------------------------------------------------------
// The memory mapping allows us to save/load types and offsets of the internal memory map. This is necessary
// since every compilation might shift the memory buffers around and we don't want to save pointers to memory
// since a re-load might not point to the same buffers. With this scheme, we can restore the proper pointers.
// -------------------------------------------------------------------------------------------------------------
#define MEM_MAP_UNKNOWN 0x00
#define MEM_MAP_CART    0x01
#define MEM_MAP_MAINMEM 0x02
#define MEM_MAP_XEMEM   0x03
#define MEM_MAP_FAST    0x04
#define MEM_MAP_BASIC   0x05

struct MemoryMap_t
{
    u8  where;
    u32 offset;
};

struct MemoryMap_t ls_mem_map[20];

void SaveMemMap()
{
    memset(ls_mem_map, 0x00, sizeof(ls_mem_map));
    
    for (int i=0; i<20; i++)
    {
        UBYTE *ptr = mem_map[i] + ((i<16) ? (i * 0x1000) : ((i-8)*0x1000));
        if ((ptr >= memory) && (ptr <= (memory+65535)))
        {
            ls_mem_map[i].where = MEM_MAP_MAINMEM;
            ls_mem_map[i].offset = ptr - memory;
        }
        else if ((ptr >= xe_mem_buffer) && (ptr <= (xe_mem_buffer+(1024*1024))))
        {
            ls_mem_map[i].where = MEM_MAP_XEMEM;
            ls_mem_map[i].offset = ptr - xe_mem_buffer;
        }
        else if ((ptr >= cart_image) && (ptr <= (cart_image+(1024*1024))))
        {
            ls_mem_map[i].where = MEM_MAP_CART;
            ls_mem_map[i].offset = ptr - cart_image;
        }
        else if ((ptr >= fast_page) && (ptr <= (fast_page+(0x1000))))
        {
            ls_mem_map[i].where = MEM_MAP_FAST;
            ls_mem_map[i].offset = ptr - fast_page;
        }            
        else if ((ptr >= ROM_basic) && (ptr <= (ROM_basic+(0x2000))))
        {
            ls_mem_map[i].where = MEM_MAP_BASIC;
            ls_mem_map[i].offset = ptr - ROM_basic;
        }
    }
}

u8 RestoreMemMap()
{
    u8 err = 0;
    for (int i=0; i<20; i++)
    {
        switch (ls_mem_map[i].where)
        {
            case MEM_MAP_MAINMEM:
                mem_map[i] = memory + ls_mem_map[i].offset - (i * 0x1000);
                break;
            case MEM_MAP_XEMEM:
                mem_map[i] = xe_mem_buffer + ls_mem_map[i].offset - (i * 0x1000);
                break;
            case MEM_MAP_CART:
                mem_map[i] = cart_image + ls_mem_map[i].offset - (i * 0x1000);
                break;
            case MEM_MAP_FAST:
                mem_map[i] = fast_page + ls_mem_map[i].offset - (i * 0x1000);
                break;
            case MEM_MAP_BASIC:
                mem_map[i] = ROM_basic + ls_mem_map[i].offset - (i * 0x1000);
                break;
            default:
                err = 1;
                break;
        }
    }
    return err;
}

u8 saved_writemap[256];
void SaveWriteMap(void)
{
    memset(saved_writemap, 0x00, sizeof(saved_writemap));
    for (int i=0; i<256; i++)
    {
        if (writemap[i] == NULL) saved_writemap[i] = 0;
        else if (writemap[i] == ROM_PutByte) saved_writemap[i] = 1;
        else saved_writemap[i] = 2;        
    }
}

void RestoreWriteMap(void)
{
    for (int i=0; i<256; i++)
    {
        if (saved_writemap[i] == 0) writemap[i] = NULL;
        else if (saved_writemap[i] == 1) writemap[i] = ROM_PutByte;
        // else do nothing... no change
    }
}
    
#define XE_NULL         0
#define XE_MAIN_MEM     1
#define XE_EXTENDED     2

u8 GetAnticXEType()
{
    if (antic_xe_ptr == NULL) return XE_NULL;
    if (antic_xe_ptr == memory + 0x4000) return XE_MAIN_MEM;
    return XE_EXTENDED;
}

void LoadAnticXE(u8 xeType, u32 offset)
{
    if (xeType == XE_NULL) antic_xe_ptr = NULL;
    else if (xeType == XE_MAIN_MEM) antic_xe_ptr = memory + 0x4000;
    else antic_xe_ptr = atarixe_memory + offset;
}


u8 spare_bytes[256];

void SaveGame(void)
{
    UWORD t0 = TIMER0_DATA;
    DIR* dir = opendir("sav");
    if (dir)
    {
        /* Directory exists. */
        closedir(dir);
    }
    else
    {
        mkdir("sav", 0777);
    }
    
    siprintf(save_filename, "sav/%s.sav", last_boot_file);
    FILE *fp = fopen(save_filename, "wb+");
    if (fp != NULL)
    {
        dsPrintValue(0,0,0, "SAVE");
        u16 rev = SAVE_FILE_REV;
        memset(spare_bytes, 0x00, 256);
        
        // Revision
        fwrite(&rev,                            sizeof(rev),                            1, fp);
        
        // Memory
        memcpy(memory+0x0000, fast_page, 0x1000);
        fwrite(memory,                          sizeof(memory),                         1, fp);
        fwrite(under_atarixl_os,                0x4000,                                 1, fp);
        fwrite(&cart809F_enabled,               sizeof(cart809F_enabled),               1, fp);
        fwrite(&cartA0BF_enabled,               sizeof(cartA0BF_enabled),               1, fp);

        SaveWriteMap();
        fwrite(saved_writemap,                  sizeof(saved_writemap),                 1, fp);
        
        SaveMemMap();
        fwrite(ls_mem_map,                      sizeof(ls_mem_map),                     1, fp);
        
        u8 xeType = GetAnticXEType();
        u32 offset = (xeType == XE_EXTENDED ? (antic_xe_ptr-atarixe_memory) : 0);
        fwrite(&xeType,                         sizeof(xeType),                         1, fp);
        fwrite(&offset,                         sizeof(offset),                         1, fp);
        fwrite(spare_bytes,                     32,                                     1, fp);
        
        // CPU
        fwrite(&regPC,                          sizeof(regPC),                          1, fp);
        fwrite(&regA,                           sizeof(regA),                           1, fp);
        fwrite(&regP,                           sizeof(regP),                           1, fp);
        fwrite(&regS,                           sizeof(regS),                           1, fp);
        fwrite(&regY,                           sizeof(regY),                           1, fp);
        fwrite(&regX,                           sizeof(regX),                           1, fp);
        fwrite(&N,                              sizeof(N),                              1, fp);
        fwrite(&Z,                              sizeof(Z),                              1, fp);
        fwrite(&C,                              sizeof(C),                              1, fp);        
        fwrite(&IRQ,                            sizeof(IRQ),                            1, fp);
        fwrite(&cim_encountered,                sizeof(cim_encountered),                1, fp);
        fwrite(spare_bytes,                     32,                                     1, fp);
        
        // ANTIC
        fwrite(ANTIC_memory,                    sizeof(ANTIC_memory),                   1, fp);
        fwrite(&DMACTL,                         sizeof(DMACTL),                         1, fp);
        fwrite(&CHACTL,                         sizeof(CHACTL),                         1, fp);
        fwrite(&dlist,                          sizeof(dlist),                          1, fp);
        fwrite(&HSCROL,                         sizeof(HSCROL),                         1, fp);
        fwrite(&VSCROL,                         sizeof(VSCROL),                         1, fp);
        fwrite(&PMBASE,                         sizeof(PMBASE),                         1, fp);
        fwrite(&CHBASE,                         sizeof(CHBASE),                         1, fp);
        fwrite(&CHBASE,                         sizeof(CHBASE),                         1, fp);
        fwrite(&NMIEN,                          sizeof(NMIEN),                          1, fp);
        fwrite(&NMIST,                          sizeof(NMIST),                          1, fp);
        fwrite(&scrn_ptr,                       sizeof(scrn_ptr),                       1, fp);
        fwrite(&break_ypos,                     sizeof(break_ypos),                     1, fp);
        fwrite(&ypos,                           sizeof(ypos),                           1, fp);
        fwrite(&wsync_halt,                     sizeof(wsync_halt),                     1, fp);
        fwrite(&screenline_cpu_clock,           sizeof(screenline_cpu_clock),           1, fp);
        fwrite(&PENH_input,                     sizeof(PENH_input),                     1, fp);
        fwrite(&PENH_input,                     sizeof(PENH_input),                     1, fp);        
        fwrite(&PENH,                           sizeof(PENH),                           1, fp);
        fwrite(&PENV,                           sizeof(PENV),                           1, fp);
        fwrite(&screenaddr,                     sizeof(screenaddr),                     1, fp);
        fwrite(&IR,                             sizeof(IR),                             1, fp);
        fwrite(&anticmode,                      sizeof(anticmode),                      1, fp);
        fwrite(&dctr,                           sizeof(dctr),                           1, fp);
        fwrite(&lastline,                       sizeof(lastline),                       1, fp);
        fwrite(&need_dl,                        sizeof(need_dl),                        1, fp);
        fwrite(&vscrol_off,                     sizeof(vscrol_off),                     1, fp);
        fwrite(&md,                             sizeof(md),                             1, fp);
        fwrite(chars_read,                      sizeof(chars_read),                     1, fp);
        fwrite(chars_displayed,                 sizeof(chars_displayed),                1, fp);
        fwrite(x_min,                           sizeof(x_min),                          1, fp);
        fwrite(ch_offset,                       sizeof(ch_offset),                      1, fp);
        fwrite(load_cycles,                     sizeof(load_cycles),                    1, fp);
        fwrite(before_cycles,                   sizeof(before_cycles),                  1, fp);
        fwrite(extra_cycles,                    sizeof(extra_cycles),                   1, fp);

#ifdef NEW_CYCLE_EXACT
        offset = (u32)cpu2antic_ptr - (u32)(&cpu2antic[0]);
        fwrite(&offset,                         sizeof(offset),                         1, fp);
        offset = (u32)antic2cpu_ptr - (u32)(&antic2cpu[0]);
        fwrite(&offset,                         sizeof(offset),                         1, fp);
        
        fwrite(&delayed_wsync,                  sizeof(delayed_wsync),                  1, fp);
        fwrite(&dmactl_changed,                 sizeof(dmactl_changed),                 1, fp);
        fwrite(&DELAYED_DMACTL,                 sizeof(DELAYED_DMACTL),                 1, fp);
        fwrite(&draw_antic_ptr_changed,         sizeof(draw_antic_ptr_changed),         1, fp);
        fwrite(&need_load,                      sizeof(need_load),                      1, fp);
        fwrite(&dmactl_bug_chdata,              sizeof(dmactl_bug_chdata),              1, fp);
        fwrite(&prevline_prior_pos,             sizeof(prevline_prior_pos),             1, fp);
        fwrite(&prevline_prior_pos,             sizeof(prevline_prior_pos),             1, fp);
        fwrite(&curline_prior_pos,              sizeof(curline_prior_pos),              1, fp);
        fwrite(&prior_curpos,                   sizeof(prior_curpos),                   1, fp);
        fwrite(&prior_val_buf,                  sizeof(prior_val_buf),                  1, fp);
        fwrite(&prior_pos_buf,                  sizeof(prior_pos_buf),                  1, fp);
#endif         

        fwrite(&left_border_chars,              sizeof(left_border_chars),              1, fp);
        fwrite(&right_border_start,             sizeof(right_border_start),             1, fp);

        fwrite(&chbase_20,                      sizeof(chbase_20),                      1, fp);
        fwrite(&invert_mask,                    sizeof(invert_mask),                    1, fp);
        fwrite(&blank_mask,                     sizeof(blank_mask),                     1, fp);
        fwrite(an_scanline,                     sizeof(an_scanline),                    1, fp);
        fwrite(blank_lookup,                    sizeof(blank_lookup),                   1, fp);
        fwrite(lookup2,                         sizeof(lookup2),                        1, fp);
        fwrite(lookup_gtia9,                    sizeof(lookup_gtia9),                   1, fp);
        fwrite(lookup_gtia11,                   sizeof(lookup_gtia11),                  1, fp);
        fwrite(playfield_lookup,                sizeof(playfield_lookup),               1, fp);
        fwrite(mode_e_an_lookup,                sizeof(mode_e_an_lookup),               1, fp);
        fwrite(cl_lookup,                       sizeof(cl_lookup),                      1, fp);
        fwrite(hires_lookup_n,                  sizeof(hires_lookup_n),                 1, fp);
        fwrite(hires_lookup_m,                  sizeof(hires_lookup_m),                 1, fp);
        fwrite(hires_lookup_l,                  sizeof(hires_lookup_l),                 1, fp);

        fwrite(&singleline,                     sizeof(singleline),                     1, fp);
        fwrite(&player_dma_enabled,             sizeof(player_dma_enabled),             1, fp);
        fwrite(&player_gra_enabled,             sizeof(player_gra_enabled),             1, fp);
        fwrite(&missile_dma_enabled,            sizeof(missile_dma_enabled),            1, fp);
        fwrite(&missile_gra_enabled,            sizeof(missile_gra_enabled),            1, fp);
        fwrite(&player_flickering,              sizeof(player_flickering),              1, fp);
        fwrite(&missile_flickering,             sizeof(missile_flickering),             1, fp);
        fwrite(&pmbase_s,                       sizeof(pmbase_s),                       1, fp);
        fwrite(&pmbase_d,                       sizeof(pmbase_d),                       1, fp);
        fwrite(&pm_dirty,                       sizeof(pm_dirty),                       1, fp);
        fwrite(&pm_lookup_ptr,                  sizeof(pm_lookup_ptr),                  1, fp);
        fwrite(pm_scanline,                     sizeof(pm_scanline),                    1, fp);
        
        UBYTE idx = get_antic_function_idx();
        fwrite(&idx,                            sizeof(idx),                            1, fp);
        idx = get_antic_0_function_idx();
        fwrite(&idx,                            sizeof(idx),                            1, fp);
        fwrite(spare_bytes,                     32,                                     1, fp);
        
        // GTIA
        fwrite(&GRAFM,                          sizeof(GRAFM),                          1, fp);
        fwrite(&GRAFP0,                         sizeof(GRAFP0),                         1, fp);
        fwrite(&GRAFP1,                         sizeof(GRAFP1),                         1, fp);
        fwrite(&GRAFP2,                         sizeof(GRAFP2),                         1, fp);
        fwrite(&GRAFP3,                         sizeof(GRAFP3),                         1, fp);
        fwrite(&HPOSP0,                         sizeof(HPOSP0),                         1, fp);
        fwrite(&HPOSP1,                         sizeof(HPOSP1),                         1, fp);
        fwrite(&HPOSP2,                         sizeof(HPOSP2),                         1, fp);
        fwrite(&HPOSP3,                         sizeof(HPOSP3),                         1, fp);
        fwrite(&HPOSM0,                         sizeof(HPOSM0),                         1, fp);
        fwrite(&HPOSM1,                         sizeof(HPOSM1),                         1, fp);
        fwrite(&HPOSM2,                         sizeof(HPOSM2),                         1, fp);
        fwrite(&HPOSM3,                         sizeof(HPOSM3),                         1, fp);
        fwrite(&SIZEP0,                         sizeof(SIZEP0),                         1, fp);
        fwrite(&SIZEP1,                         sizeof(SIZEP1),                         1, fp);
        fwrite(&SIZEP2,                         sizeof(SIZEP2),                         1, fp);
        fwrite(&SIZEP3,                         sizeof(SIZEP3),                         1, fp);
        fwrite(&SIZEM,                          sizeof(SIZEM),                          1, fp);
        fwrite(&COLPM0,                         sizeof(COLPM0),                         1, fp);
        fwrite(&COLPM1,                         sizeof(COLPM1),                         1, fp);
        fwrite(&COLPM2,                         sizeof(COLPM2),                         1, fp);
        fwrite(&COLPM3,                         sizeof(COLPM3),                         1, fp);
        fwrite(&COLPF0,                         sizeof(COLPF0),                         1, fp);
        fwrite(&COLPF1,                         sizeof(COLPF1),                         1, fp);
        fwrite(&COLPF2,                         sizeof(COLPF2),                         1, fp);
        fwrite(&COLPF3,                         sizeof(COLPF3),                         1, fp);
        fwrite(&COLBK,                          sizeof(COLBK),                          1, fp);
        fwrite(&GRACTL,                         sizeof(GRACTL),                         1, fp);
        fwrite(&M0PL,                           sizeof(M0PL),                           1, fp);
        fwrite(&M1PL,                           sizeof(M1PL),                           1, fp);
        fwrite(&M2PL,                           sizeof(M2PL),                           1, fp);
        fwrite(&M3PL,                           sizeof(M3PL),                           1, fp);
        fwrite(&P0PL,                           sizeof(P0PL),                           1, fp);
        fwrite(&P1PL,                           sizeof(P1PL),                           1, fp);
        fwrite(&P2PL,                           sizeof(P2PL),                           1, fp);
        fwrite(&P3PL,                           sizeof(P3PL),                           1, fp);

#ifdef NEW_CYCLE_EXACT
        fwrite(&P1PL_T,                         sizeof(P1PL_T),                         1, fp);
        fwrite(&P2PL_T,                         sizeof(P2PL_T),                         1, fp);
        fwrite(&P3PL_T,                         sizeof(P3PL_T),                         1, fp);
        fwrite(&M0PL_T,                         sizeof(M0PL_T),                         1, fp);
        fwrite(&M1PL_T,                         sizeof(M1PL_T),                         1, fp);
        fwrite(&M2PL_T,                         sizeof(M2PL_T),                         1, fp);
        fwrite(&M3PL_T,                         sizeof(M3PL_T),                         1, fp);
        fwrite(&collision_curpos,               sizeof(collision_curpos),               1, fp);
        fwrite(&hitclr_pos,                     sizeof(hitclr_pos),                     1, fp);
#endif       
                
        fwrite(&PRIOR,                          sizeof(PRIOR),                          1, fp);
        fwrite(&VDELAY,                         sizeof(VDELAY),                         1, fp);
        fwrite(&POTENA,                         sizeof(POTENA),                         1, fp);
                
        fwrite(&atari_speaker,                  sizeof(atari_speaker),                  1, fp);
        fwrite(&consol_index,                   sizeof(consol_index),                   1, fp);
        fwrite(&consol_mask,                    sizeof(consol_mask),                    1, fp);
                
        fwrite(consol_table,                    sizeof(consol_table),                   1, fp);
        fwrite(TRIG,                            sizeof(TRIG),                           1, fp);
        fwrite(TRIG_latch,                      sizeof(TRIG_latch),                     1, fp);        
        
        fwrite(hposp_ptr,                       sizeof(hposp_ptr),                      1, fp);
        fwrite(hposm_ptr,                       sizeof(hposm_ptr),                      1, fp);
        fwrite(hposp_mask,                      sizeof(hposp_mask),                     1, fp);
        fwrite(grafp_ptr,                       sizeof(grafp_ptr),                      1, fp);
        fwrite(global_sizem,                    sizeof(global_sizem),                   1, fp);
        fwrite(PM_Width,                        sizeof(PM_Width),                       1, fp);       
        fwrite(spare_bytes,                     32,                                     1, fp);
        
        // PIA
        fwrite(&PACTL,                          sizeof(PACTL),                          1, fp);
        fwrite(&PBCTL,                          sizeof(PBCTL),                          1, fp);
        fwrite(&PORTA,                          sizeof(PORTA),                          1, fp);
        fwrite(&PORTB,                          sizeof(PORTB),                          1, fp);
        fwrite(&PORTA_mask,                     sizeof(PORTA_mask),                     1, fp);
        fwrite(&PORTB_mask,                     sizeof(PORTB_mask),                     1, fp);
        fwrite(PORT_input,                      sizeof(PORT_input),                     1, fp);
        fwrite(&xe_bank,                        sizeof(xe_bank),                        1, fp);
        fwrite(&selftest_enabled,               sizeof(selftest_enabled),               1, fp);
        fwrite(&PIA_CA2,                        sizeof(PIA_CA2),                        1, fp);
        fwrite(&PIA_CA2_negpending,             sizeof(PIA_CA2_negpending),             1, fp);
        fwrite(&PIA_CA2_pospending,             sizeof(PIA_CA2_pospending),             1, fp);
        fwrite(&PIA_CB2,                        sizeof(PIA_CB2),                        1, fp);
        fwrite(&PIA_CB2_negpending,             sizeof(PIA_CB2_negpending),             1, fp);
        fwrite(&PIA_CB2_pospending,             sizeof(PIA_CB2_pospending),             1, fp);
        fwrite(&PIA_IRQ,                        sizeof(PIA_IRQ),                        1, fp);        
        fwrite(spare_bytes,                     25,                                     1, fp);
        
        
        // SIO
        fwrite(SIO_drive_status,                sizeof(SIO_drive_status),               1, fp);
        fwrite(CommandFrame,                    sizeof(CommandFrame),                   1, fp);
        fwrite(DataBuffer,                      sizeof(DataBuffer),                     1, fp);
        fwrite(&SIO_last_drive,                 sizeof(SIO_last_drive),                 1, fp);
        fwrite(&CommandIndex,                   sizeof(CommandIndex),                   1, fp);
        fwrite(&DataIndex,                      sizeof(DataIndex),                      1, fp);
        fwrite(&TransferStatus,                 sizeof(TransferStatus),                 1, fp);
        fwrite(&ExpectedBytes,                  sizeof(ExpectedBytes),                  1, fp);
        fwrite(spare_bytes,                     32,                                     1, fp);

        // POKEY
        fwrite(&pokeyBufIdx,                    sizeof(pokeyBufIdx),                    1, fp);
        fwrite(pokey_buffer,                    sizeof(pokey_buffer),                   1, fp);
        fwrite(&KBCODE,                         sizeof(KBCODE),                         1, fp);
        fwrite(&SERIN,                          sizeof(SERIN),                          1, fp);
        fwrite(&IRQST,                          sizeof(IRQST),                          1, fp);
        fwrite(&IRQEN,                          sizeof(IRQEN),                          1, fp);
        fwrite(&SKSTAT,                         sizeof(SKSTAT),                         1, fp);
        fwrite(&SKCTLS,                         sizeof(SKCTLS),                         1, fp);

        fwrite(&DELAYED_SERIN_IRQ,              sizeof(DELAYED_SERIN_IRQ),              1, fp);
        fwrite(&DELAYED_SEROUT_IRQ,             sizeof(DELAYED_SEROUT_IRQ),             1, fp);
        fwrite(&DELAYED_XMTDONE_IRQ,            sizeof(DELAYED_XMTDONE_IRQ),            1, fp);
        
        fwrite(AUDF,                            sizeof(AUDF),                           1, fp);
        fwrite(AUDC,                            sizeof(AUDC),                           1, fp);
        fwrite(AUDCTL,                          sizeof(AUDCTL),                         1, fp);
        fwrite(DivNIRQ,                         sizeof(DivNIRQ),                        1, fp);
        fwrite(DivNMax,                         sizeof(DivNMax),                        1, fp);
        fwrite(Base_mult,                       sizeof(Base_mult),                      1, fp);
        fwrite(POT_input,                       sizeof(POT_input),                      1, fp);
        fwrite(PCPOT_input,                     sizeof(PCPOT_input),                    1, fp);

        fwrite(&POT_all,                        sizeof(POT_all),                        1, fp);
        fwrite(&pot_scanline,                   sizeof(pot_scanline),                   1, fp);
        fwrite(&random_scanline_counter,        sizeof(random_scanline_counter),        1, fp);
        

        fwrite(AUDV,                            sizeof(AUDV),                           1, fp);
        fwrite(Outbit,                          sizeof(Outbit),                         1, fp);
        fwrite(Outvol,                          sizeof(Outvol),                         1, fp);
        fwrite(Div_n_cnt,                       sizeof(Div_n_cnt),                      1, fp);
        fwrite(Div_n_max,                       sizeof(Div_n_max),                      1, fp);
        
        fwrite(&P4,                             sizeof(P4),                             1, fp);
        fwrite(&P5,                             sizeof(P5),                             1, fp);
        fwrite(&P9,                             sizeof(P9),                             1, fp);
        fwrite(&P17,                            sizeof(P17),                            1, fp);
        fwrite(&Samp_n_max,                     sizeof(Samp_n_max),                     1, fp);
        fwrite(Samp_n_cnt,                      sizeof(Samp_n_cnt),                     1, fp);
        fwrite(spare_bytes,                     32,                                     1, fp);
        
        //A8DS
        fwrite(&gTotalAtariFrames,              sizeof(gTotalAtariFrames),              1, fp);
        fwrite(&emu_state,                      sizeof(emu_state),                      1, fp);
        fwrite(&atari_frames,                   sizeof(atari_frames),                   1, fp);
        fwrite(&sound_idx,                      sizeof(sound_idx),                      1, fp);
        fwrite(&myPokeyBufIdx,                  sizeof(myPokeyBufIdx),                  1, fp);
        fwrite(&t0,                             sizeof(t0),                             1, fp);
        fwrite(spare_bytes,                     32,                                     1, fp);
        
        // XE Memory - this is potentially big so we only save used memory which might shrink this considerably
        u32 mem_used = XE_MemUsed();
        fwrite(&mem_used,                       sizeof(mem_used),                       1, fp);
        fwrite(xe_mem_buffer,                   sizeof(UBYTE),                          mem_used, fp); 
        
        // Spare Bytes - Reduce this as needed to eat into spare memory
        fwrite(spare_bytes,                     256,                                    1, fp); 

        fclose(fp);
    }
    else dsPrintValue(0,0,0, "ERR ");
    
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    dsPrintValue(0,0,0, "    ");
}

void LoadGame(void)
{
    u8 err = false;
    UWORD t0 = 0;
    
    siprintf(save_filename, "sav/%s.sav", last_boot_file);
    FILE *fp = fopen(save_filename, "rb");
    if (fp != NULL)
    {
        u16 rev=0;
        // Revision
        fread(&rev,                                sizeof(rev),                            1, fp);
        
        if (rev == SAVE_FILE_REV)
        {            
            dsPrintValue(0,0,0, "LOAD");
            // Memory
            fread(memory,                          sizeof(memory),                         1, fp);
            memcpy(fast_page, memory+0x0000, 0x1000);
            fread(under_atarixl_os,                0x4000,                                 1, fp);
            fread(&cart809F_enabled,               sizeof(cart809F_enabled),               1, fp);
            fread(&cartA0BF_enabled,               sizeof(cartA0BF_enabled),               1, fp);
            
            fread(saved_writemap,                  sizeof(saved_writemap),                 1, fp);
            RestoreWriteMap();
            
            fread(ls_mem_map,                      sizeof(ls_mem_map),                     1, fp);
            err = RestoreMemMap();

            u8 xeType = 0;
            u32 offset = 0;
            fread(&xeType,                         sizeof(xeType),                         1, fp);
            fread(&offset,                         sizeof(offset),                         1, fp);
            LoadAnticXE(xeType, offset);
            fread(spare_bytes,                     32,                                     1, fp);

            // CPU
            fread(&regPC,                          sizeof(regPC),                          1, fp);
            fread(&regA,                           sizeof(regA),                           1, fp);
            fread(&regP,                           sizeof(regP),                           1, fp);
            fread(&regS,                           sizeof(regS),                           1, fp);
            fread(&regY,                           sizeof(regY),                           1, fp);
            fread(&regX,                           sizeof(regX),                           1, fp);
            fread(&N,                              sizeof(N),                              1, fp);
            fread(&Z,                              sizeof(Z),                              1, fp);
            fread(&C,                              sizeof(C),                              1, fp);        
            fread(&IRQ,                            sizeof(IRQ),                            1, fp);
            fread(&cim_encountered,                sizeof(cim_encountered),                1, fp);
            fread(spare_bytes,                     32,                                     1, fp);

            // ANTIC
            fread(ANTIC_memory,                    sizeof(ANTIC_memory),                   1, fp);
            fread(&DMACTL,                         sizeof(DMACTL),                         1, fp);
            fread(&CHACTL,                         sizeof(CHACTL),                         1, fp);
            fread(&dlist,                          sizeof(dlist),                          1, fp);
            fread(&HSCROL,                         sizeof(HSCROL),                         1, fp);
            fread(&VSCROL,                         sizeof(VSCROL),                         1, fp);
            fread(&PMBASE,                         sizeof(PMBASE),                         1, fp);
            fread(&CHBASE,                         sizeof(CHBASE),                         1, fp);
            fread(&CHBASE,                         sizeof(CHBASE),                         1, fp);
            fread(&NMIEN,                          sizeof(NMIEN),                          1, fp);
            fread(&NMIST,                          sizeof(NMIST),                          1, fp);
            fread(&scrn_ptr,                       sizeof(scrn_ptr),                       1, fp);
            fread(&break_ypos,                     sizeof(break_ypos),                     1, fp);
            fread(&ypos,                           sizeof(ypos),                           1, fp);
            fread(&wsync_halt,                     sizeof(wsync_halt),                     1, fp);
            fread(&screenline_cpu_clock,           sizeof(screenline_cpu_clock),           1, fp);
            fread(&PENH_input,                     sizeof(PENH_input),                     1, fp);
            fread(&PENH_input,                     sizeof(PENH_input),                     1, fp);        
            fread(&PENH,                           sizeof(PENH),                           1, fp);
            fread(&PENV,                           sizeof(PENV),                           1, fp);
            fread(&screenaddr,                     sizeof(screenaddr),                     1, fp);
            fread(&IR,                             sizeof(IR),                             1, fp);
            fread(&anticmode,                      sizeof(anticmode),                      1, fp);
            fread(&dctr,                           sizeof(dctr),                           1, fp);
            fread(&lastline,                       sizeof(lastline),                       1, fp);
            fread(&need_dl,                        sizeof(need_dl),                        1, fp);
            fread(&vscrol_off,                     sizeof(vscrol_off),                     1, fp);
            fread(&md,                             sizeof(md),                             1, fp);
            fread(chars_read,                      sizeof(chars_read),                     1, fp);
            fread(chars_displayed,                 sizeof(chars_displayed),                1, fp);
            fread(x_min,                           sizeof(x_min),                          1, fp);
            fread(ch_offset,                       sizeof(ch_offset),                      1, fp);
            fread(load_cycles,                     sizeof(load_cycles),                    1, fp);
            fread(before_cycles,                   sizeof(before_cycles),                  1, fp);
            fread(extra_cycles,                    sizeof(extra_cycles),                   1, fp);

#ifdef NEW_CYCLE_EXACT            
            fread(&offset,                         sizeof(offset),                         1, fp);
            cpu2antic_ptr = &cpu2antic[0] + offset;
            fread(&offset,                         sizeof(offset),                         1, fp);
            antic2cpu_ptr = &antic2cpu[0] + offset;

            fread(&delayed_wsync,                  sizeof(delayed_wsync),                  1, fp);
            fread(&dmactl_changed,                 sizeof(dmactl_changed),                 1, fp);
            fread(&DELAYED_DMACTL,                 sizeof(DELAYED_DMACTL),                 1, fp);
            fread(&draw_antic_ptr_changed,         sizeof(draw_antic_ptr_changed),         1, fp);
            fread(&need_load,                      sizeof(need_load),                      1, fp);
            fread(&dmactl_bug_chdata,              sizeof(dmactl_bug_chdata),              1, fp);
            fread(&prevline_prior_pos,             sizeof(prevline_prior_pos),             1, fp);
            fread(&prevline_prior_pos,             sizeof(prevline_prior_pos),             1, fp);
            fread(&curline_prior_pos,              sizeof(curline_prior_pos),              1, fp);
            fread(&prior_curpos,                   sizeof(prior_curpos),                   1, fp);
            fread(&prior_val_buf,                  sizeof(prior_val_buf),                  1, fp);
            fread(&prior_pos_buf,                  sizeof(prior_pos_buf),                  1, fp);            
#endif            

            fread(&left_border_chars,              sizeof(left_border_chars),              1, fp);
            fread(&right_border_start,             sizeof(right_border_start),             1, fp);

            fread(&chbase_20,                      sizeof(chbase_20),                      1, fp);
            fread(&invert_mask,                    sizeof(invert_mask),                    1, fp);
            fread(&blank_mask,                     sizeof(blank_mask),                     1, fp);
            fread(an_scanline,                     sizeof(an_scanline),                    1, fp);
            fread(blank_lookup,                    sizeof(blank_lookup),                   1, fp);
            fread(lookup2,                         sizeof(lookup2),                        1, fp);
            fread(lookup_gtia9,                    sizeof(lookup_gtia9),                   1, fp);
            fread(lookup_gtia11,                   sizeof(lookup_gtia11),                  1, fp);
            fread(playfield_lookup,                sizeof(playfield_lookup),               1, fp);
            fread(mode_e_an_lookup,                sizeof(mode_e_an_lookup),               1, fp);
            fread(cl_lookup,                       sizeof(cl_lookup),                      1, fp);
            fread(hires_lookup_n,                  sizeof(hires_lookup_n),                 1, fp);
            fread(hires_lookup_m,                  sizeof(hires_lookup_m),                 1, fp);
            fread(hires_lookup_l,                  sizeof(hires_lookup_l),                 1, fp);

            fread(&singleline,                     sizeof(singleline),                     1, fp);
            fread(&player_dma_enabled,             sizeof(player_dma_enabled),             1, fp);
            fread(&player_gra_enabled,             sizeof(player_gra_enabled),             1, fp);
            fread(&missile_dma_enabled,            sizeof(missile_dma_enabled),            1, fp);
            fread(&missile_gra_enabled,            sizeof(missile_gra_enabled),            1, fp);
            fread(&player_flickering,              sizeof(player_flickering),              1, fp);
            fread(&missile_flickering,             sizeof(missile_flickering),             1, fp);
            fread(&pmbase_s,                       sizeof(pmbase_s),                       1, fp);
            fread(&pmbase_d,                       sizeof(pmbase_d),                       1, fp);
            fread(&pm_dirty,                       sizeof(pm_dirty),                       1, fp);
            fread(&pm_lookup_ptr,                  sizeof(pm_lookup_ptr),                  1, fp);
            fread(pm_scanline,                     sizeof(pm_scanline),                    1, fp);

            UBYTE idx = 0;
            fread(&idx,                            sizeof(idx),                            1, fp);
            set_antic_function_by_idx(idx);
            fread(&idx,                            sizeof(idx),                            1, fp);
            set_antic_0_function_by_idx(idx);

            fread(spare_bytes,                     32,                                     1, fp);
            
            // GTIA
            fread(&GRAFM,                          sizeof(GRAFM),                          1, fp);
            fread(&GRAFP0,                         sizeof(GRAFP0),                         1, fp);
            fread(&GRAFP1,                         sizeof(GRAFP1),                         1, fp);
            fread(&GRAFP2,                         sizeof(GRAFP2),                         1, fp);
            fread(&GRAFP3,                         sizeof(GRAFP3),                         1, fp);
            fread(&HPOSP0,                         sizeof(HPOSP0),                         1, fp);
            fread(&HPOSP1,                         sizeof(HPOSP1),                         1, fp);
            fread(&HPOSP2,                         sizeof(HPOSP2),                         1, fp);
            fread(&HPOSP3,                         sizeof(HPOSP3),                         1, fp);
            fread(&HPOSM0,                         sizeof(HPOSM0),                         1, fp);
            fread(&HPOSM1,                         sizeof(HPOSM1),                         1, fp);
            fread(&HPOSM2,                         sizeof(HPOSM2),                         1, fp);
            fread(&HPOSM3,                         sizeof(HPOSM3),                         1, fp);
            fread(&SIZEP0,                         sizeof(SIZEP0),                         1, fp);
            fread(&SIZEP1,                         sizeof(SIZEP1),                         1, fp);
            fread(&SIZEP2,                         sizeof(SIZEP2),                         1, fp);
            fread(&SIZEP3,                         sizeof(SIZEP3),                         1, fp);
            fread(&SIZEM,                          sizeof(SIZEM),                          1, fp);
            fread(&COLPM0,                         sizeof(COLPM0),                         1, fp);
            fread(&COLPM1,                         sizeof(COLPM1),                         1, fp);
            fread(&COLPM2,                         sizeof(COLPM2),                         1, fp);
            fread(&COLPM3,                         sizeof(COLPM3),                         1, fp);
            fread(&COLPF0,                         sizeof(COLPF0),                         1, fp);
            fread(&COLPF1,                         sizeof(COLPF1),                         1, fp);
            fread(&COLPF2,                         sizeof(COLPF2),                         1, fp);
            fread(&COLPF3,                         sizeof(COLPF3),                         1, fp);
            fread(&COLBK,                          sizeof(COLBK),                          1, fp);
            fread(&GRACTL,                         sizeof(GRACTL),                         1, fp);
            fread(&M0PL,                           sizeof(M0PL),                           1, fp);
            fread(&M1PL,                           sizeof(M1PL),                           1, fp);
            fread(&M2PL,                           sizeof(M2PL),                           1, fp);
            fread(&M3PL,                           sizeof(M3PL),                           1, fp);
            fread(&P0PL,                           sizeof(P0PL),                           1, fp);
            fread(&P1PL,                           sizeof(P1PL),                           1, fp);
            fread(&P2PL,                           sizeof(P2PL),                           1, fp);
            fread(&P3PL,                           sizeof(P3PL),                           1, fp);

#ifdef NEW_CYCLE_EXACT
            fread(&P1PL_T,                         sizeof(P1PL_T),                         1, fp);
            fread(&P2PL_T,                         sizeof(P2PL_T),                         1, fp);
            fread(&P3PL_T,                         sizeof(P3PL_T),                         1, fp);
            fread(&M0PL_T,                         sizeof(M0PL_T),                         1, fp);
            fread(&M1PL_T,                         sizeof(M1PL_T),                         1, fp);
            fread(&M2PL_T,                         sizeof(M2PL_T),                         1, fp);
            fread(&M3PL_T,                         sizeof(M3PL_T),                         1, fp);
            fread(&collision_curpos,               sizeof(collision_curpos),               1, fp);
            fread(&hitclr_pos,                     sizeof(hitclr_pos),                     1, fp);
#endif       

            fread(&PRIOR,                          sizeof(PRIOR),                          1, fp);
            fread(&VDELAY,                         sizeof(VDELAY),                         1, fp);
            fread(&POTENA,                         sizeof(POTENA),                         1, fp);

            fread(&atari_speaker,                  sizeof(atari_speaker),                  1, fp);
            fread(&consol_index,                   sizeof(consol_index),                   1, fp);
            fread(&consol_mask,                    sizeof(consol_mask),                    1, fp);

            fread(consol_table,                    sizeof(consol_table),                   1, fp);
            fread(TRIG,                            sizeof(TRIG),                           1, fp);
            fread(TRIG_latch,                      sizeof(TRIG_latch),                     1, fp);        

            fread(hposp_ptr,                       sizeof(hposp_ptr),                      1, fp);
            fread(hposm_ptr,                       sizeof(hposm_ptr),                      1, fp);
            fread(hposp_mask,                      sizeof(hposp_mask),                     1, fp);
            fread(grafp_ptr,                       sizeof(grafp_ptr),                      1, fp);
            fread(global_sizem,                    sizeof(global_sizem),                   1, fp);
            fread(PM_Width,                        sizeof(PM_Width),                       1, fp);       
            fread(spare_bytes,                     32,                                     1, fp);

            // PIA
            fread(&PACTL,                          sizeof(PACTL),                          1, fp);
            fread(&PBCTL,                          sizeof(PBCTL),                          1, fp);
            fread(&PORTA,                          sizeof(PORTA),                          1, fp);
            fread(&PORTB,                          sizeof(PORTB),                          1, fp);
            fread(&PORTA_mask,                     sizeof(PORTA_mask),                     1, fp);
            fread(&PORTB_mask,                     sizeof(PORTB_mask),                     1, fp);
            fread(PORT_input,                      sizeof(PORT_input),                     1, fp);
            fread(&xe_bank,                        sizeof(xe_bank),                        1, fp);
            fread(&selftest_enabled,               sizeof(selftest_enabled),               1, fp);
            fread(&PIA_CA2,                        sizeof(PIA_CA2),                        1, fp);
            fread(&PIA_CA2_negpending,             sizeof(PIA_CA2_negpending),             1, fp);
            fread(&PIA_CA2_pospending,             sizeof(PIA_CA2_pospending),             1, fp);
            fread(&PIA_CB2,                        sizeof(PIA_CB2),                        1, fp);
            fread(&PIA_CB2_negpending,             sizeof(PIA_CB2_negpending),             1, fp);
            fread(&PIA_CB2_pospending,             sizeof(PIA_CB2_pospending),             1, fp);
            fread(&PIA_IRQ,                        sizeof(PIA_IRQ),                        1, fp);        
            fread(spare_bytes,                     25,                                     1, fp);

            // SIO
            fread(SIO_drive_status,                sizeof(SIO_drive_status),               1, fp);
            fread(CommandFrame,                    sizeof(CommandFrame),                   1, fp);
            fread(DataBuffer,                      sizeof(DataBuffer),                     1, fp);
            fread(&SIO_last_drive,                 sizeof(SIO_last_drive),                 1, fp);
            fread(&CommandIndex,                   sizeof(CommandIndex),                   1, fp);
            fread(&DataIndex,                      sizeof(DataIndex),                      1, fp);
            fread(&TransferStatus,                 sizeof(TransferStatus),                 1, fp);
            fread(&ExpectedBytes,                  sizeof(ExpectedBytes),                  1, fp);
            fread(spare_bytes,                     32,                                     1, fp);
            
            // POKEY
            fread(&pokeyBufIdx,                    sizeof(pokeyBufIdx),                    1, fp);
            fread(pokey_buffer,                    sizeof(pokey_buffer),                   1, fp);
            fread(&KBCODE,                         sizeof(KBCODE),                         1, fp);
            fread(&SERIN,                          sizeof(SERIN),                          1, fp);
            fread(&IRQST,                          sizeof(IRQST),                          1, fp);
            fread(&IRQEN,                          sizeof(IRQEN),                          1, fp);
            fread(&SKSTAT,                         sizeof(SKSTAT),                         1, fp);
            fread(&SKCTLS,                         sizeof(SKCTLS),                         1, fp);

            fread(&DELAYED_SERIN_IRQ,              sizeof(DELAYED_SERIN_IRQ),              1, fp);
            fread(&DELAYED_SEROUT_IRQ,             sizeof(DELAYED_SEROUT_IRQ),             1, fp);
            fread(&DELAYED_XMTDONE_IRQ,            sizeof(DELAYED_XMTDONE_IRQ),            1, fp);

            fread(AUDF,                            sizeof(AUDF),                           1, fp);
            fread(AUDC,                            sizeof(AUDC),                           1, fp);
            fread(AUDCTL,                          sizeof(AUDCTL),                         1, fp);
            fread(DivNIRQ,                         sizeof(DivNIRQ),                        1, fp);
            fread(DivNMax,                         sizeof(DivNMax),                        1, fp);
            fread(Base_mult,                       sizeof(Base_mult),                      1, fp);
            fread(POT_input,                       sizeof(POT_input),                      1, fp);
            fread(PCPOT_input,                     sizeof(PCPOT_input),                    1, fp);

            fread(&POT_all,                        sizeof(POT_all),                        1, fp);
            fread(&pot_scanline,                   sizeof(pot_scanline),                   1, fp);
            fread(&random_scanline_counter,        sizeof(random_scanline_counter),        1, fp);
            
            fread(AUDV,                            sizeof(AUDV),                           1, fp);
            fread(Outbit,                          sizeof(Outbit),                         1, fp);
            fread(Outvol,                          sizeof(Outvol),                         1, fp);
            fread(Div_n_cnt,                       sizeof(Div_n_cnt),                      1, fp);
            fread(Div_n_max,                       sizeof(Div_n_max),                      1, fp);

            fread(&P4,                             sizeof(P4),                             1, fp);
            fread(&P5,                             sizeof(P5),                             1, fp);
            fread(&P9,                             sizeof(P9),                             1, fp);
            fread(&P17,                            sizeof(P17),                            1, fp);
            fread(&Samp_n_max,                     sizeof(Samp_n_max),                     1, fp);
            fread(Samp_n_cnt,                      sizeof(Samp_n_cnt),                     1, fp);
            fread(spare_bytes,                     32,                                     1, fp);

            //A8DS
            fread(&gTotalAtariFrames,              sizeof(gTotalAtariFrames),              1, fp);
            fread(&emu_state,                      sizeof(emu_state),                      1, fp);
            fread(&atari_frames,                   sizeof(atari_frames),                   1, fp);
            fread(&sound_idx,                      sizeof(sound_idx),                      1, fp);
            fread(&myPokeyBufIdx,                  sizeof(myPokeyBufIdx),                  1, fp);
            fread(&t0,                             sizeof(t0),                             1, fp);
            fread(spare_bytes,                     32,                                     1, fp);

            // XE Memory - this is potentially big so we only save used memory which might shrink this considerably
            u32 mem_used = 0;
            fread(&mem_used,                        sizeof(mem_used),                      1, fp);
            fread(xe_mem_buffer,                    sizeof(UBYTE),                         mem_used, fp); 
            
            // Spare Bytes - Reduce this as needed to eat into spare memory
            fread(spare_bytes,                     256,                                    1, fp); 
            
        } else err = true;
        
        pm_dirty = true;
        fclose(fp);
    } else err = true;
    
    if (err) dsPrintValue(0,0,0, "ERR ");
    
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    dsPrintValue(0,0,0, "    ");
    if (!err) TIMER0_DATA = t0;
}


// End of file
