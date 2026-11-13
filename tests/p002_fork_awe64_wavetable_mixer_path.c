/*
 * Research-only execution of the pinned fork's exact AWE64 Gold wavetable
 * mixer function.  The static function is included from the unmodified
 * submodule so this test cannot drift into a locally reimplemented DSP.
 *
 * The stubs below isolate the post-EMU8000 boundary: the input array is the
 * already-rendered emu8k.buffer, while PC/DSP/MPU side effects are irrelevant
 * to the wavetable mixer arithmetic under test.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../third_party/86box-awe32/src/sound/snd_sb.c"

int wavetable_pos_global;

void
emu8k_update(emu8k_t *emu8k)
{
    (void) emu8k;
}

void
emu8k_reset_buffer(emu8k_t *emu8k)
{
    memset(emu8k->buffer, 0, sizeof(emu8k->buffer));
}

/* Mixer-reset dependencies outside the wavetable audio path. */
void sb_dsp_setirq(sb_dsp_t *dsp, int value) { (void) dsp; (void) value; }
void sb_dsp_setdma8(sb_dsp_t *dsp, int value) { (void) dsp; (void) value; }
void sb_dsp_setdma16(sb_dsp_t *dsp, int value) { (void) dsp; (void) value; }
void sb_dsp_setdma16_enabled(sb_dsp_t *dsp, int value) { (void) dsp; (void) value; }
void sb_dsp_setdma16_translate(sb_dsp_t *dsp, int value) { (void) dsp; (void) value; }
void
sb_update_mask(sb_dsp_t *dsp, int irq8, int irq16, int irq401)
{
    (void) dsp; (void) irq8; (void) irq16; (void) irq401;
}
void
gameport_remap(void *priv, uint16_t address)
{
    (void) priv; (void) address;
}
void
mpu401_change_addr(mpu_t *mpu, uint16_t address)
{
    (void) mpu; (void) address;
}

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "P002_AWE64_MIXER_FAIL line=%d condition=%s\n", \
            __LINE__, #x); return 1; } } while (0)

static void
write_mixer(sb_t *sb, uint8_t reg, uint8_t value)
{
    sb_ct1745_mixer_write(4, reg, sb);
    sb_ct1745_mixer_write(5, value, sb);
}

static void
load_chip_output(sb_t *sb, const int32_t *input, uint16_t frames)
{
    memcpy(sb->emu8k.buffer, input,
           (size_t) frames * 2u * sizeof(input[0]));
    sb->emu8k.pos = frames;
    wavetable_pos_global = frames;
}

int
main(void)
{
    sb_t sb;
    memset(&sb, 0, sizeof(sb));
    sb_ct1745_mixer_reset(&sb);

    /* Exact CT1745 reset values used by sb_awe32_pnp_init(), including
       SB_AWE64_GOLD.  0xF8 indexes attenuation-table position 31. */
    CHECK(sb.mixer_sb16.regs[0x30] == 0xf8);
    CHECK(sb.mixer_sb16.regs[0x31] == 0xf8);
    CHECK(sb.mixer_sb16.regs[0x34] == 0xf8);
    CHECK(sb.mixer_sb16.regs[0x35] == 0xf8);
    CHECK(sb.mixer_sb16.regs[0x41] == 0x00);
    CHECK(sb.mixer_sb16.regs[0x42] == 0x00);
    CHECK(sb.mixer_sb16.regs[0x44] == 0x80);
    CHECK(sb.mixer_sb16.regs[0x45] == 0x80);
    CHECK(sb.mixer_sb16.regs[0x46] == 0x80);
    CHECK(sb.mixer_sb16.regs[0x47] == 0x80);
    CHECK(sb.mixer_sb16.fm_l == 32767.0 / 32768.0);
    CHECK(sb.mixer_sb16.fm_r == 32767.0 / 32768.0);
    CHECK(sb.mixer_sb16.master_l == 32767.0 / 32768.0);
    CHECK(sb.mixer_sb16.master_r == 32767.0 / 32768.0);
    CHECK(sb.mixer_sb16.output_gain_L == 1.0);
    CHECK(sb.mixer_sb16.output_gain_R == 1.0);
    CHECK(sb.mixer_sb16.bass_l == 8 && sb.mixer_sb16.bass_r == 8);
    CHECK(sb.mixer_sb16.treble_l == 8 && sb.mixer_sb16.treble_r == 8);

    /* Execute the exact original static fork function with samples both
       inside and outside PCM16.  It scales/truncates but does not clip. */
    static const int32_t input[] = {
        -40000, 40000, -32768, 32767, -1, 1, 12345, -12345
    };
    static const int32_t expected_default[] = {
        -39997, 39997, -32766, 32765, 0, 0, 12344, -12344
    };
    int32_t output[8] = { 0 };
    load_chip_output(&sb, input, 4);
    sb_get_wavetable_buffer_sb16_awe32(output, 4, &sb);
    CHECK(memcmp(output, expected_default, sizeof(output)) == 0);
    CHECK(output[0] < -32768 && output[1] > 32767);
    for (size_t i = 0; i < 8; i++)
        CHECK(sb.emu8k.buffer[i] == 0);

    /* Negative control: mutate the real CT1745 left wavetable/FM register.
       Register zero is the table's minimum (-62 dB), not a hard mute.  The
       right channel must remain on the reset path. */
    write_mixer(&sb, 0x34, 0x00);
    memset(output, 0, sizeof(output));
    load_chip_output(&sb, input, 4);
    sb_get_wavetable_buffer_sb16_awe32(output, 4, &sb);
    CHECK(output[0] == -30 && output[2] == -24 &&
          output[4] == 0 && output[6] == 9);
    CHECK(output[1] == expected_default[1]);
    CHECK(output[3] == expected_default[3]);
    CHECK(output[5] == expected_default[5]);
    CHECK(output[7] == expected_default[7]);

    /* Restore volume, then prove the optional CT1745 bass branch is live.
       No replacement filter is implemented here; this calls filters.h
       through the original snd_sb.c function. */
    write_mixer(&sb, 0x34, 0xf8);
    write_mixer(&sb, 0x46, 0xf0);
    memset(output, 0, sizeof(output));
    load_chip_output(&sb, input, 4);
    sb_get_wavetable_buffer_sb16_awe32(output, 4, &sb);
    CHECK(output[0] != expected_default[0] ||
          output[2] != expected_default[2] ||
          output[4] != expected_default[4] ||
          output[6] != expected_default[6]);
    CHECK(output[1] == expected_default[1]);
    CHECK(output[3] == expected_default[3]);
    CHECK(output[5] == expected_default[5]);
    CHECK(output[7] == expected_default[7]);

    puts("P002_PINNED_FORK_AWE64_GOLD_WAVETABLE_MIXER_PASS");
    puts("chip_input=synthetic_int32 post_mixer=exact_snd_sb_static_function");
    puts("default_ct1745=two_32767_over_32768_gains tone=neutral output_gain=1");
    puts("post_mixer_clipping=NONE sink_policy=SEPARATE");
    return 0;
}
