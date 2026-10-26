// SPDX-License-Identifier: MIT
// C++ port of FrankHUN88453/CTR-NitroFueled-PC 96b4aa5474b1d42b0d9f383349dc091ea9a2d385
// tools/gcn.py (MIT, ctrport/d3d/LICENSE.FrankHUN88453): GCN2 (PS4 "Liverpool") instruction
// decoder and the PS4 shader footer. Decoded fields keep the Python field names and order,
// because the translator's heuristics iterate them.
#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gcn {

struct DecodeError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline std::map<int, std::string> table(std::initializer_list<std::pair<const int, std::string>> l) { return l; }

inline const std::map<int, std::string>& SOP2() {
    static const auto t = table({{0, "s_add_u32"}, {1, "s_sub_u32"}, {2, "s_add_i32"}, {3, "s_sub_i32"}, {4, "s_addc_u32"},
        {5, "s_subb_u32"}, {6, "s_min_i32"}, {7, "s_min_u32"}, {8, "s_max_i32"}, {9, "s_max_u32"},
        {10, "s_cselect_b32"}, {11, "s_cselect_b64"}, {14, "s_and_b32"}, {15, "s_and_b64"},
        {16, "s_or_b32"}, {17, "s_or_b64"}, {18, "s_xor_b32"}, {19, "s_xor_b64"}, {20, "s_andn2_b32"},
        {21, "s_andn2_b64"}, {22, "s_orn2_b32"}, {23, "s_orn2_b64"}, {24, "s_nand_b32"},
        {25, "s_nand_b64"}, {26, "s_nor_b32"}, {27, "s_nor_b64"}, {28, "s_xnor_b32"}, {29, "s_xnor_b64"},
        {30, "s_lshl_b32"}, {31, "s_lshl_b64"}, {32, "s_lshr_b32"}, {33, "s_lshr_b64"},
        {34, "s_ashr_i32"}, {35, "s_ashr_i64"}, {36, "s_bfm_b32"}, {37, "s_bfm_b64"}, {38, "s_mul_i32"},
        {39, "s_bfe_u32"}, {40, "s_bfe_i32"}, {41, "s_bfe_u64"}, {42, "s_bfe_i64"},
        {43, "s_cbranch_g_fork"}, {44, "s_absdiff_i32"}});
    return t;
}
inline const std::map<int, std::string>& SOPK() {
    static const auto t = table({{0, "s_movk_i32"}, {2, "s_cmovk_i32"}, {3, "s_cmpk_eq_i32"}, {4, "s_cmpk_lg_i32"},
        {5, "s_cmpk_gt_i32"}, {6, "s_cmpk_ge_i32"}, {7, "s_cmpk_lt_i32"}, {8, "s_cmpk_le_i32"},
        {9, "s_cmpk_eq_u32"}, {10, "s_cmpk_lg_u32"}, {11, "s_cmpk_gt_u32"}, {12, "s_cmpk_ge_u32"},
        {13, "s_cmpk_lt_u32"}, {14, "s_cmpk_le_u32"}, {15, "s_addk_i32"}, {16, "s_mulk_i32"},
        {17, "s_cbranch_i_fork"}, {18, "s_getreg_b32"}, {19, "s_setreg_b32"},
        {21, "s_setreg_imm32_b32"}});
    return t;
}
inline const std::map<int, std::string>& SOP1() {
    static const auto t = table({{3, "s_mov_b32"}, {4, "s_mov_b64"}, {5, "s_cmov_b32"}, {6, "s_cmov_b64"}, {7, "s_not_b32"},
        {8, "s_not_b64"}, {9, "s_wqm_b32"}, {10, "s_wqm_b64"}, {11, "s_brev_b32"}, {12, "s_brev_b64"},
        {13, "s_bcnt0_i32_b32"}, {14, "s_bcnt0_i32_b64"}, {15, "s_bcnt1_i32_b32"},
        {16, "s_bcnt1_i32_b64"}, {17, "s_ff0_i32_b32"}, {18, "s_ff0_i32_b64"}, {19, "s_ff1_i32_b32"},
        {20, "s_ff1_i32_b64"}, {21, "s_flbit_i32_b32"}, {22, "s_flbit_i32_b64"}, {23, "s_flbit_i32"},
        {24, "s_flbit_i32_i64"}, {25, "s_sext_i32_i8"}, {26, "s_sext_i32_i16"}, {27, "s_bitset0_b32"},
        {28, "s_bitset0_b64"}, {29, "s_bitset1_b32"}, {30, "s_bitset1_b64"}, {31, "s_getpc_b64"},
        {32, "s_setpc_b64"}, {33, "s_swappc_b64"}, {34, "s_rfe_b64"}, {36, "s_and_saveexec_b64"},
        {37, "s_or_saveexec_b64"}, {38, "s_xor_saveexec_b64"}, {39, "s_andn2_saveexec_b64"},
        {40, "s_orn2_saveexec_b64"}, {41, "s_nand_saveexec_b64"}, {42, "s_nor_saveexec_b64"},
        {43, "s_xnor_saveexec_b64"}, {44, "s_quadmask_b32"}, {45, "s_quadmask_b64"},
        {46, "s_movrels_b32"}, {47, "s_movrels_b64"}, {48, "s_movreld_b32"}, {49, "s_movreld_b64"},
        {50, "s_cbranch_join"}, {52, "s_abs_i32"}, {53, "s_mov_fed_b32"}});
    return t;
}
inline const std::map<int, std::string>& SOPC() {
    static const auto t = table({{0, "s_cmp_eq_i32"}, {1, "s_cmp_lg_i32"}, {2, "s_cmp_gt_i32"}, {3, "s_cmp_ge_i32"},
        {4, "s_cmp_lt_i32"}, {5, "s_cmp_le_i32"}, {6, "s_cmp_eq_u32"}, {7, "s_cmp_lg_u32"},
        {8, "s_cmp_gt_u32"}, {9, "s_cmp_ge_u32"}, {10, "s_cmp_lt_u32"}, {11, "s_cmp_le_u32"},
        {12, "s_bitcmp0_b32"}, {13, "s_bitcmp1_b32"}, {14, "s_bitcmp0_b64"}, {15, "s_bitcmp1_b64"},
        {16, "s_setvskip"}});
    return t;
}
inline const std::map<int, std::string>& SOPP() {
    static const auto t = table({{0, "s_nop"}, {1, "s_endpgm"}, {2, "s_branch"}, {4, "s_cbranch_scc0"}, {5, "s_cbranch_scc1"},
        {6, "s_cbranch_vccz"}, {7, "s_cbranch_vccnz"}, {8, "s_cbranch_execz"}, {9, "s_cbranch_execnz"},
        {10, "s_barrier"}, {12, "s_waitcnt"}, {13, "s_sethalt"}, {14, "s_sleep"}, {15, "s_setprio"},
        {16, "s_sendmsg"}, {17, "s_sendmsghalt"}, {18, "s_trap"}, {19, "s_icache_inv"},
        {20, "s_incperflevel"}, {21, "s_decperflevel"}, {22, "s_ttracedata"}});
    return t;
}
inline const std::map<int, std::string>& SMRD() {
    static const auto t = table({{0, "s_load_dword"}, {1, "s_load_dwordx2"}, {2, "s_load_dwordx4"}, {3, "s_load_dwordx8"},
        {4, "s_load_dwordx16"}, {8, "s_buffer_load_dword"}, {9, "s_buffer_load_dwordx2"},
        {10, "s_buffer_load_dwordx4"}, {11, "s_buffer_load_dwordx8"}, {12, "s_buffer_load_dwordx16"},
        {29, "s_dcache_inv_vol"}, {30, "s_memtime"}, {31, "s_dcache_inv"}});
    return t;
}
inline const std::map<int, std::string>& VOP2() {
    static const auto t = table({{0, "v_cndmask_b32"}, {1, "v_readlane_b32"}, {2, "v_writelane_b32"}, {3, "v_add_f32"},
        {4, "v_sub_f32"}, {5, "v_subrev_f32"}, {6, "v_mac_legacy_f32"}, {7, "v_mul_legacy_f32"},
        {8, "v_mul_f32"}, {9, "v_mul_i32_i24"}, {10, "v_mul_hi_i32_i24"}, {11, "v_mul_u32_u24"},
        {12, "v_mul_hi_u32_u24"}, {13, "v_min_legacy_f32"}, {14, "v_max_legacy_f32"},
        {15, "v_min_f32"}, {16, "v_max_f32"}, {17, "v_min_i32"}, {18, "v_max_i32"}, {19, "v_min_u32"},
        {20, "v_max_u32"}, {21, "v_lshr_b32"}, {22, "v_lshrrev_b32"}, {23, "v_ashr_i32"},
        {24, "v_ashrrev_i32"}, {25, "v_lshl_b32"}, {26, "v_lshlrev_b32"}, {27, "v_and_b32"},
        {28, "v_or_b32"}, {29, "v_xor_b32"}, {30, "v_bfm_b32"}, {31, "v_mac_f32"}, {32, "v_madmk_f32"},
        {33, "v_madak_f32"}, {34, "v_bcnt_u32_b32"}, {35, "v_mbcnt_lo_u32_b32"},
        {36, "v_mbcnt_hi_u32_b32"}, {37, "v_add_i32"}, {38, "v_sub_i32"}, {39, "v_subrev_i32"},
        {40, "v_addc_u32"}, {41, "v_subb_u32"}, {42, "v_subbrev_u32"}, {43, "v_ldexp_f32"},
        {44, "v_cvt_pkaccum_u8_f32"}, {45, "v_cvt_pknorm_i16_f32"}, {46, "v_cvt_pknorm_u16_f32"},
        {47, "v_cvt_pkrtz_f16_f32"}, {48, "v_cvt_pk_u16_u32"}, {49, "v_cvt_pk_i16_i32"}});
    return t;
}
inline const std::map<int, std::string>& VOP1() {
    static const auto t = table({{0, "v_nop"}, {1, "v_mov_b32"}, {2, "v_readfirstlane_b32"}, {3, "v_cvt_i32_f64"},
        {4, "v_cvt_f64_i32"}, {5, "v_cvt_f32_i32"}, {6, "v_cvt_f32_u32"}, {7, "v_cvt_u32_f32"},
        {8, "v_cvt_i32_f32"}, {9, "v_mov_fed_b32"}, {10, "v_cvt_f16_f32"}, {11, "v_cvt_f32_f16"},
        {12, "v_cvt_rpi_i32_f32"}, {13, "v_cvt_flr_i32_f32"}, {14, "v_cvt_off_f32_i4"},
        {15, "v_cvt_f32_f64"}, {16, "v_cvt_f64_f32"}, {17, "v_cvt_f32_ubyte0"},
        {18, "v_cvt_f32_ubyte1"}, {19, "v_cvt_f32_ubyte2"}, {20, "v_cvt_f32_ubyte3"},
        {21, "v_cvt_u32_f64"}, {22, "v_cvt_f64_u32"}, {23, "v_trunc_f64"}, {24, "v_ceil_f64"},
        {25, "v_rndne_f64"}, {26, "v_floor_f64"}, {32, "v_fract_f32"}, {33, "v_trunc_f32"},
        {34, "v_ceil_f32"}, {35, "v_rndne_f32"}, {36, "v_floor_f32"}, {37, "v_exp_f32"},
        {38, "v_log_clamp_f32"}, {39, "v_log_f32"}, {40, "v_rcp_clamp_f32"}, {41, "v_rcp_legacy_f32"},
        {42, "v_rcp_f32"}, {43, "v_rcp_iflag_f32"}, {44, "v_rsq_clamp_f32"}, {45, "v_rsq_legacy_f32"},
        {46, "v_rsq_f32"}, {47, "v_rcp_f64"}, {48, "v_rcp_clamp_f64"}, {49, "v_rsq_f64"},
        {50, "v_rsq_clamp_f64"}, {51, "v_sqrt_f32"}, {52, "v_sqrt_f64"}, {53, "v_sin_f32"},
        {54, "v_cos_f32"}, {55, "v_not_b32"}, {56, "v_bfrev_b32"}, {57, "v_ffbh_u32"}, {58, "v_ffbl_b32"},
        {59, "v_ffbh_i32"}, {60, "v_frexp_exp_i32_f64"}, {61, "v_frexp_mant_f64"},
        {62, "v_fract_f64"}, {63, "v_frexp_exp_i32_f32"}, {64, "v_frexp_mant_f32"},
        {65, "v_clrexcp"}, {66, "v_movreld_b32"}, {67, "v_movrels_b32"}, {68, "v_movrelsd_b32"},
        {69, "v_log_legacy_f32"}, {70, "v_exp_legacy_f32"}});
    return t;
}
inline const std::map<int, std::string>& VOPC() {
    static const auto t = [] {
        std::map<int, std::string> m;
        const char* fc[] = {"f", "lt", "eq", "le", "gt", "lg", "ge", "o", "u", "nge", "nlg", "ngt", "nle", "neq", "nlt", "tru"};
        const char* ic[] = {"f", "lt", "eq", "le", "gt", "ne", "ge", "t"};
        for (int i = 0; i < 16; i++) {
            const std::string c = fc[i];
            m[0x00 + i] = "v_cmp_" + c + "_f32";
            m[0x10 + i] = "v_cmpx_" + c + "_f32";
            m[0x20 + i] = "v_cmp_" + c + "_f64";
            m[0x30 + i] = "v_cmpx_" + c + "_f64";
            m[0x40 + i] = "v_cmps_" + c + "_f32";
            m[0x50 + i] = "v_cmpsx_" + c + "_f32";
            m[0x60 + i] = "v_cmps_" + c + "_f64";
            m[0x70 + i] = "v_cmpsx_" + c + "_f64";
        }
        for (int i = 0; i < 8; i++) {
            const std::string c = ic[i];
            m[0x80 + i] = "v_cmp_" + c + "_i32";
            m[0x90 + i] = "v_cmpx_" + c + "_i32";
            m[0xA0 + i] = "v_cmp_" + c + "_i64";
            m[0xB0 + i] = "v_cmpx_" + c + "_i64";
            m[0xC0 + i] = "v_cmp_" + c + "_u32";
            m[0xD0 + i] = "v_cmpx_" + c + "_u32";
            m[0xE0 + i] = "v_cmp_" + c + "_u64";
            m[0xF0 + i] = "v_cmpx_" + c + "_u64";
        }
        m[0x88] = "v_cmp_class_f32";
        m[0x98] = "v_cmpx_class_f32";
        m[0xA8] = "v_cmp_class_f64";
        m[0xB8] = "v_cmpx_class_f64";
        return m;
    }();
    return t;
}
inline const std::map<int, std::string>& VOP3_ONLY() {
    static const auto t = table({{320, "v_mad_legacy_f32"}, {321, "v_mad_f32"}, {322, "v_mad_i32_i24"},
        {323, "v_mad_u32_u24"}, {324, "v_cubeid_f32"}, {325, "v_cubesc_f32"},
        {326, "v_cubetc_f32"}, {327, "v_cubema_f32"}, {328, "v_bfe_u32"}, {329, "v_bfe_i32"},
        {330, "v_bfi_b32"}, {331, "v_fma_f32"}, {332, "v_fma_f64"}, {333, "v_lerp_u8"},
        {334, "v_alignbit_b32"}, {335, "v_alignbyte_b32"}, {336, "v_mullit_f32"},
        {337, "v_min3_f32"}, {338, "v_min3_i32"}, {339, "v_min3_u32"}, {340, "v_max3_f32"},
        {341, "v_max3_i32"}, {342, "v_max3_u32"}, {343, "v_med3_f32"}, {344, "v_med3_i32"},
        {345, "v_med3_u32"}, {346, "v_sad_u8"}, {347, "v_sad_hi_u8"}, {348, "v_sad_u16"},
        {349, "v_sad_u32"}, {350, "v_cvt_pk_u8_f32"}, {351, "v_div_fixup_f32"},
        {352, "v_div_fixup_f64"}, {353, "v_lshl_b64"}, {354, "v_lshr_b64"}, {355, "v_ashr_i64"},
        {356, "v_add_f64"}, {357, "v_mul_f64"}, {358, "v_min_f64"}, {359, "v_max_f64"},
        {360, "v_ldexp_f64"}, {361, "v_mul_lo_u32"}, {362, "v_mul_hi_u32"}, {363, "v_mul_lo_i32"},
        {364, "v_mul_hi_i32"}, {365, "v_div_scale_f32"}, {366, "v_div_scale_f64"},
        {367, "v_div_fmas_f32"}, {368, "v_div_fmas_f64"}, {369, "v_msad_u8"},
        {370, "v_qsad_pk_u16_u8"}, {371, "v_mqsad_pk_u16_u8"}, {372, "v_trig_preop_f64"},
        {373, "v_mqsad_u32_u8"}, {374, "v_mad_u64_u32"}, {375, "v_mad_i64_i32"}});
    return t;
}
inline const std::map<int, std::string>& MUBUF() {
    static const auto t = table({{0, "buffer_load_format_x"}, {1, "buffer_load_format_xy"}, {2, "buffer_load_format_xyz"},
        {3, "buffer_load_format_xyzw"}, {4, "buffer_store_format_x"}, {5, "buffer_store_format_xy"},
        {6, "buffer_store_format_xyz"}, {7, "buffer_store_format_xyzw"}, {8, "buffer_load_ubyte"},
        {9, "buffer_load_sbyte"}, {10, "buffer_load_ushort"}, {11, "buffer_load_sshort"},
        {12, "buffer_load_dword"}, {13, "buffer_load_dwordx2"}, {14, "buffer_load_dwordx4"},
        {15, "buffer_load_dwordx3"}, {24, "buffer_store_byte"}, {26, "buffer_store_short"},
        {28, "buffer_store_dword"}, {29, "buffer_store_dwordx2"}, {30, "buffer_store_dwordx4"},
        {31, "buffer_store_dwordx3"}, {50, "buffer_atomic_add"}, {112, "buffer_wbinvl1_vol"},
        {113, "buffer_wbinvl1"}});
    return t;
}
inline const std::map<int, std::string>& MTBUF() {
    static const auto t = table({{0, "tbuffer_load_format_x"}, {1, "tbuffer_load_format_xy"}, {2, "tbuffer_load_format_xyz"},
        {3, "tbuffer_load_format_xyzw"}, {4, "tbuffer_store_format_x"},
        {5, "tbuffer_store_format_xy"}, {6, "tbuffer_store_format_xyz"},
        {7, "tbuffer_store_format_xyzw"}});
    return t;
}
inline const std::map<int, std::string>& MIMG() {
    static const auto t = [] {
        std::map<int, std::string> m = {{0, "image_load"}, {1, "image_load_mip"}, {2, "image_load_pck"}, {3, "image_load_pck_sgn"},
            {4, "image_load_mip_pck"}, {5, "image_load_mip_pck_sgn"}, {8, "image_store"},
            {9, "image_store_mip"}, {10, "image_store_pck"}, {11, "image_store_mip_pck"},
            {14, "image_get_resinfo"}, {96, "image_get_lod"}};
        const char* sample[] = {"", "_cl", "_d", "_d_cl", "_l", "_b", "_b_cl", "_lz", "_c", "_c_cl", "_c_d",
                                "_c_d_cl", "_c_l", "_c_b", "_c_b_cl", "_c_lz"};
        for (int i = 0; i < 16; i++) {
            m[32 + i] = std::string("image_sample") + sample[i];
            m[48 + i] = std::string("image_sample") + sample[i] + "_o";
        }
        for (int i = 0; i < 16; i++) {
            if (std::string(sample[i]).find("_d") == std::string::npos) {
                m[64 + i] = std::string("image_gather4") + sample[i];
                m[80 + i] = std::string("image_gather4") + sample[i] + "_o";
            }
        }
        const char* cd[] = {"_cd", "_cd_cl", "_c_cd", "_c_cd_cl", "_cd_o", "_cd_cl_o", "_c_cd_o", "_c_cd_cl_o"};
        for (int i = 0; i < 8; i++) m[104 + i] = std::string("image_sample") + cd[i];
        return m;
    }();
    return t;
}
inline const std::map<int, std::string>& DS() {
    static const auto t = table({{0, "ds_add_u32"}, {13, "ds_write_b32"}, {14, "ds_write2_b32"}, {53, "ds_swizzle_b32"},
        {54, "ds_read_b32"}, {55, "ds_read2_b32"}, {78, "ds_write_b64"}, {118, "ds_read_b64"}});
    return t;
}
inline std::string name_of(const std::map<int, std::string>& t, int op, const std::string& prefix) {
    auto it = t.find(op);
    return it != t.end() ? it->second : prefix + std::to_string(op);
}

// Source operand (gcn.py Operand): s<n>, v<n>, a literal, an inline int / float or a special.
struct Operand {
    enum Kind { S, V, Lit, Int, Float, Special } kind = Int;
    int64_t value = 0;     // S / V / Lit / Int
    const char* fstr = "";  // Float: Python repr of the inline constant
    std::string special;    // Special
    bool none = false;      // literal operand of an instruction without one (VOP3 / MUBUF soffset)
};

inline Operand src_operand(uint32_t v, std::optional<uint32_t> literal) {
    Operand o;
    static const std::map<uint32_t, const char*> special = {{106, "vcc_lo"}, {107, "vcc_hi"}, {124, "m0"}, {126, "exec_lo"},
                                                            {127, "exec_hi"}, {251, "vccz"}, {252, "execz"}, {253, "scc"},
                                                            {254, "lds_direct"}};
    static const std::map<uint32_t, const char*> inline_f = {{240, "0.5"}, {241, "-0.5"}, {242, "1.0"}, {243, "-1.0"},
                                                             {244, "2.0"}, {245, "-2.0"}, {246, "4.0"}, {247, "-4.0"}};
    if (v <= 103) {
        o.kind = Operand::S, o.value = v;
    } else if (special.count(v)) {
        o.kind = Operand::Special, o.special = special.at(v);
    } else if (v >= 128 && v <= 192) {
        o.kind = Operand::Int, o.value = (int64_t)v - 128;
    } else if (v >= 193 && v <= 208) {
        o.kind = Operand::Int, o.value = 192 - (int64_t)v;
    } else if (inline_f.count(v)) {
        o.kind = Operand::Float, o.fstr = inline_f.at(v);
    } else if (v == 255) {
        o.kind = Operand::Lit;
        if (literal) o.value = *literal;
        else o.none = true;
    } else if (v >= 256 && v <= 511) {
        o.kind = Operand::V, o.value = v - 256;
    } else {
        o.kind = Operand::Special, o.special = "src" + std::to_string(v);
    }
    return o;
}

// Inst.f of gcn.py: integer fields and operands, in insertion order.
struct Field {
    std::string key;
    enum { Int, Op, List, Str } type = Int;
    int64_t i = 0;
    Operand op;
    std::vector<int64_t> list;
    std::string s;
};

struct Inst {
    int64_t pc = 0;
    std::string fmt;
    int op = 0;
    std::string name;
    int size = 4;
    std::vector<Field> f;

    const Field* find(const std::string& k) const {
        for (const auto& x : f)
            if (x.key == k) return &x;
        return nullptr;
    }
    bool has(const std::string& k) const { return find(k) != nullptr; }
    int64_t I(const std::string& k) const {
        const Field* x = find(k);
        if (!x || x->type != Field::Int) throw std::runtime_error("missing field " + k + " in " + name);
        return x->i;
    }
    std::optional<int64_t> geti(const std::string& k) const {
        const Field* x = find(k);
        if (!x || x->type != Field::Int) return std::nullopt;
        return x->i;
    }
    const Operand& O(const std::string& k) const {
        const Field* x = find(k);
        if (!x || x->type != Field::Op) throw std::runtime_error("missing operand " + k + " in " + name);
        return x->op;
    }
    const Operand* opt(const std::string& k) const {
        const Field* x = find(k);
        return x && x->type == Field::Op ? &x->op : nullptr;
    }
    std::string S(const std::string& k) const {
        const Field* x = find(k);
        return x && x->type == Field::Str ? x->s : std::string();
    }
    void set(const std::string& k, int64_t v) {
        for (auto& x : f)
            if (x.key == k) {
                x.type = Field::Int, x.i = v;
                return;
            }
        Field x;
        x.key = k, x.i = v;
        f.push_back(x);
    }
    void set(const std::string& k, const Operand& o) {
        Field x;
        x.key = k, x.type = Field::Op, x.op = o;
        f.push_back(x);
    }
    void set_str(const std::string& k, const std::string& s) {
        Field x;
        x.key = k, x.type = Field::Str, x.s = s;
        f.push_back(x);
    }
};

inline Inst decode_one(int64_t pc, uint32_t w0, uint32_t w1) {
    Inst i;
    i.pc = pc;
    const uint32_t top2 = w0 >> 30;
    if (top2 == 2) {  // scalar ALU family
        const uint32_t top9 = w0 >> 23;
        if (top9 == 0x17D) {
            const int op = (w0 >> 8) & 0xFF;
            const uint32_t s0 = w0 & 0xFF;
            i.fmt = "SOP1", i.op = op, i.name = name_of(SOP1(), op, "sop1_"), i.size = s0 == 255 ? 8 : 4;
            i.set("sdst", (w0 >> 16) & 0x7F);
            i.set("ssrc0", src_operand(s0, w1));
            return i;
        }
        if (top9 == 0x17E) {
            const int op = (w0 >> 16) & 0x7F;
            const uint32_t s0 = w0 & 0xFF, s1 = (w0 >> 8) & 0xFF;
            i.fmt = "SOPC", i.op = op, i.name = name_of(SOPC(), op, "sopc_"), i.size = (s0 == 255 || s1 == 255) ? 8 : 4;
            i.set("ssrc0", src_operand(s0, w1));
            i.set("ssrc1", src_operand(s1, w1));
            return i;
        }
        if (top9 == 0x17F) {
            const int op = (w0 >> 16) & 0x7F;
            i.fmt = "SOPP", i.op = op, i.name = name_of(SOPP(), op, "sopp_"), i.size = 4;
            i.set("simm16", w0 & 0xFFFF);
            return i;
        }
        if ((w0 >> 28) == 0xB) {
            const int op = (w0 >> 23) & 0x1F;
            i.fmt = "SOPK", i.op = op, i.name = name_of(SOPK(), op, "sopk_"), i.size = op == 21 ? 8 : 4;
            i.set("sdst", (w0 >> 16) & 0x7F);
            i.set("simm16", w0 & 0xFFFF);
            if (op == 21) i.set("lit", w1);
            return i;
        }
        const int op = (w0 >> 23) & 0x7F;
        const uint32_t s0 = w0 & 0xFF, s1 = (w0 >> 8) & 0xFF;
        i.fmt = "SOP2", i.op = op, i.name = name_of(SOP2(), op, "sop2_"), i.size = (s0 == 255 || s1 == 255) ? 8 : 4;
        i.set("sdst", (w0 >> 16) & 0x7F);
        i.set("ssrc0", src_operand(s0, w1));
        i.set("ssrc1", src_operand(s1, w1));
        return i;
    }
    if ((w0 >> 31) == 0) {  // VOP2 / VOP1 / VOPC
        const uint32_t top7 = w0 >> 25;
        const uint32_t s0 = w0 & 0x1FF;
        int size = s0 == 255 ? 8 : 4;
        if (top7 == 0x3F) {
            const int op = (w0 >> 9) & 0xFF;
            i.fmt = "VOP1", i.op = op, i.name = name_of(VOP1(), op, "vop1_"), i.size = size;
            i.set("vdst", (w0 >> 17) & 0xFF);
            i.set("src0", src_operand(s0, w1));
            return i;
        }
        if (top7 == 0x3E) {
            const int op = (w0 >> 17) & 0xFF;
            i.fmt = "VOPC", i.op = op, i.name = name_of(VOPC(), op, "vopc_"), i.size = size;
            i.set("src0", src_operand(s0, w1));
            i.set("vsrc1", (w0 >> 9) & 0xFF);
            return i;
        }
        const int op = (w0 >> 25) & 0x3F;
        if (op == 32 || op == 33) size = 8;  // madmk / madak carry a literal
        i.fmt = "VOP2", i.op = op, i.name = name_of(VOP2(), op, "vop2_"), i.size = size;
        i.set("vdst", (w0 >> 17) & 0xFF);
        i.set("src0", src_operand(s0, w1));
        i.set("vsrc1", (w0 >> 9) & 0xFF);
        if (op == 32 || op == 33) i.set("lit", w1);
        return i;
    }
    const uint32_t top6 = w0 >> 26;
    if ((w0 >> 27) == 0x18) {  // SMRD
        const int op = (w0 >> 22) & 0x1F;
        const uint32_t imm = (w0 >> 8) & 1, off = w0 & 0xFF;
        i.fmt = "SMRD", i.op = op, i.name = name_of(SMRD(), op, "smrd_"), i.size = (!imm && off == 255) ? 8 : 4;
        i.set("sdst", (w0 >> 15) & 0x7F);
        i.set("sbase", ((w0 >> 9) & 0x3F) * 2);
        i.set("imm", imm);
        i.set("offset", off);
        if (i.size == 8) i.set("lit", w1);
        return i;
    }
    if (top6 == 0x34) {  // VOP3
        const int op = (w0 >> 17) & 0x1FF;
        std::string name, base;
        if (op < 256) name = name_of(VOPC(), op, "vopc_"), base = "VOPC";
        else if (op < 320) name = name_of(VOP2(), op - 256, "vop2_"), base = "VOP2";
        else if (op < 384) name = name_of(VOP3_ONLY(), op, "vop3_"), base = "VOP3";
        else name = name_of(VOP1(), op - 384, "vop1_"), base = "VOP1";
        i.fmt = "VOP3", i.op = op, i.name = name, i.size = 8;
        const bool sdst_form = base == "VOPC" || name == "v_add_i32" || name == "v_sub_i32" || name == "v_subrev_i32" ||
                               name == "v_addc_u32" || name == "v_subb_u32" || name == "v_subbrev_u32" ||
                               name == "v_div_scale_f32" || name == "v_div_scale_f64";
        i.set_str("base", base);
        i.set("vdst", w0 & 0xFF);
        i.set("src0", src_operand(w1 & 0x1FF, std::nullopt));
        i.set("src1", src_operand((w1 >> 9) & 0x1FF, std::nullopt));
        i.set("src2", src_operand((w1 >> 18) & 0x1FF, std::nullopt));
        i.set("omod", (w1 >> 27) & 3);
        i.set("neg", (w1 >> 29) & 7);
        if (base == "VOPC") {
            i.set("sdst", w0 & 0xFF);
        } else if (sdst_form) {
            i.set("sdst", (w0 >> 8) & 0x7F);
        } else {
            i.set("abs", (w0 >> 8) & 7);
            i.set("clamp", (w0 >> 11) & 1);
        }
        return i;
    }
    if (top6 == 0x32) {  // VINTRP
        const int op = (w0 >> 16) & 3;
        static const char* names[] = {"v_interp_p1_f32", "v_interp_p2_f32", "v_interp_mov_f32"};
        i.fmt = "VINTRP", i.op = op, i.name = op < 3 ? names[op] : "vintrp_" + std::to_string(op), i.size = 4;
        i.set("vdst", (w0 >> 18) & 0xFF);
        i.set("attr", (w0 >> 10) & 0x3F);
        i.set("chan", (w0 >> 8) & 3);
        i.set("vsrc", w0 & 0xFF);
        return i;
    }
    if (top6 == 0x36) {  // DS
        const int op = (w0 >> 18) & 0xFF;
        i.fmt = "DS", i.op = op, i.name = name_of(DS(), op, "ds_"), i.size = 8;
        i.set("offset0", w0 & 0xFF);
        i.set("offset1", (w0 >> 8) & 0xFF);
        i.set("gds", (w0 >> 17) & 1);
        i.set("addr", w1 & 0xFF);
        i.set("data0", (w1 >> 8) & 0xFF);
        i.set("data1", (w1 >> 16) & 0xFF);
        i.set("vdst", w1 >> 24);
        return i;
    }
    if (top6 == 0x38 || top6 == 0x3A) {  // MUBUF / MTBUF
        const bool mt = top6 == 0x3A;
        const int op = mt ? (w0 >> 16) & 7 : (w0 >> 18) & 0x7F;
        i.fmt = mt ? "MTBUF" : "MUBUF", i.op = op, i.name = name_of(mt ? MTBUF() : MUBUF(), op, "buf_"), i.size = 8;
        i.set("offset", w0 & 0xFFF);
        i.set("offen", (w0 >> 12) & 1);
        i.set("idxen", (w0 >> 13) & 1);
        i.set("glc", (w0 >> 14) & 1);
        i.set("addr64", (w0 >> 15) & 1);
        i.set("vaddr", w1 & 0xFF);
        i.set("vdata", (w1 >> 8) & 0xFF);
        i.set("srsrc", ((w1 >> 16) & 0x1F) * 4);
        i.set("soffset", src_operand(w1 >> 24, std::nullopt));
        if (mt) {
            i.set("dfmt", (w0 >> 19) & 0xF);
            i.set("nfmt", (w0 >> 23) & 7);
        }
        return i;
    }
    if (top6 == 0x3C) {  // MIMG
        const int op = (w0 >> 18) & 0x7F;
        i.fmt = "MIMG", i.op = op, i.name = name_of(MIMG(), op, "mimg_"), i.size = 8;
        i.set("dmask", (w0 >> 8) & 0xF);
        i.set("unorm", (w0 >> 12) & 1);
        i.set("da", (w0 >> 14) & 1);
        i.set("vaddr", w1 & 0xFF);
        i.set("vdata", (w1 >> 8) & 0xFF);
        i.set("srsrc", ((w1 >> 16) & 0x1F) * 4);
        i.set("ssamp", ((w1 >> 21) & 0x1F) * 4);
        return i;
    }
    if (top6 == 0x3E) {  // EXP
        i.fmt = "EXP", i.op = 0, i.name = "exp", i.size = 8;
        i.set("en", w0 & 0xF);
        i.set("tgt", (w0 >> 4) & 0x3F);
        i.set("compr", (w0 >> 10) & 1);
        i.set("done", (w0 >> 11) & 1);
        i.set("vm", (w0 >> 12) & 1);
        Field l;
        l.key = "vsrc", l.type = Field::List, l.list = {w1 & 0xFF, (w1 >> 8) & 0xFF, (w1 >> 16) & 0xFF, w1 >> 24};
        i.f.push_back(l);
        return i;
    }
    char b[64];
    snprintf(b, sizeof(b), "%04llx: unknown encoding %08x", (unsigned long long)pc, w0);
    throw DecodeError(b);
}

inline std::vector<Inst> decode(const std::vector<uint8_t>& code, int64_t start, int64_t end) {
    std::vector<Inst> out;
    int64_t pc = start;
    while (pc < end) {
        if (pc + 4 > (int64_t)code.size()) throw std::runtime_error("unpack_from out of range");
        uint32_t w0, w1 = 0;
        memcpy(&w0, code.data() + pc, 4);
        if (pc + 8 <= (int64_t)code.size()) memcpy(&w1, code.data() + pc + 4, 4);
        Inst i = decode_one(pc, w0, w1);
        pc += i.size;
        out.push_back(std::move(i));
    }
    return out;
}

// PS4 shader binary footer ("OrbShdr"): program length and usage slots.
struct Footer {
    int64_t offset = -1;
    uint32_t length = 0;
};
inline int64_t rfind(const std::vector<uint8_t>& code, const char* s) {
    const size_t n = strlen(s);
    if (code.size() < n) return -1;
    for (size_t p = code.size() - n + 1; p-- > 0;)
        if (!memcmp(code.data() + p, s, n)) return (int64_t)p;
    return -1;
}
inline int64_t find(const std::vector<uint8_t>& code, const char* s) {
    const size_t n = strlen(s);
    for (size_t p = 0; p + n <= code.size(); p++)
        if (!memcmp(code.data() + p, s, n)) return (int64_t)p;
    return -1;
}
inline std::optional<Footer> parse_footer(const std::vector<uint8_t>& code) {
    const int64_t p = rfind(code, "OrbShdr");
    if (p < 0) return std::nullopt;
    if (p + 12 > (int64_t)code.size()) throw std::runtime_error("footer truncated");
    uint32_t bits;
    memcpy(&bits, code.data() + p + 8, 4);
    return Footer{p, bits >> 8};
}

}  // namespace gcn
