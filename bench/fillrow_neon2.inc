static void _fillRow(uint32_t* dst, uint32_t* src, uint32_t len, uint32_t color, uint8_t opacity, bool direct)
{
    uint32_t x = 0;
#if defined(THORVG_AVX_SUPPORT)
    // copy A of each pixel into both 16-bit halves of its lane
    const auto alphaIdx = _mm256_setr_epi8(3, -1, 3, -1, 7, -1, 7, -1, 11, -1, 11, -1, 15, -1, 15, -1,
                                           3, -1, 3, -1, 7, -1, 7, -1, 11, -1, 11, -1, 15, -1, 15, -1);
    const auto opacityVec = _mm256_set1_epi16(opacity);
    const auto bias = _mm256_set1_epi16(510);
    const auto full = _mm256_set1_epi16(257);
    const auto mask = _mm256_set1_epi32(0xff00ff00);
    const auto colorEven = _mm256_set1_epi32((color & 0x00ff00ff) << 8);
    const auto colorOdd = _mm256_set1_epi32((color >> 8) & 0x00ff00ff);

    for (; len - x >= 8; x += 8) {
        auto pixels = reinterpret_cast<__m256i*>(dst + x);
        auto alpha = _mm256_shuffle_epi8(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + x)), alphaIdx);
        // ((opacity * alpha + 255) >> 8) + 1 = (alpha * opacity + 511) >> 8, avg intrinsic can add without the 16-bit overflow
        auto factors = _mm256_srli_epi16(_mm256_avg_epu16(_mm256_mullo_epi16(alpha, opacityVec), bias), 7);
        auto result = _mm256_or_si256(_mm256_mulhi_epu16(colorEven, factors), _mm256_and_si256(_mm256_mullo_epi16(colorOdd, factors), mask));
        if (direct) {
            // + ALPHA_BLEND(dst, 255 - a) with 256 - a = 257 - factors
            auto target = _mm256_loadu_si256(pixels);
            auto inverse = _mm256_sub_epi16(full, factors);
            auto even = _mm256_mulhi_epu16(_mm256_slli_epi16(target, 8), inverse);
            auto odd = _mm256_and_si256(_mm256_mullo_epi16(_mm256_srli_epi16(target, 8), inverse), mask);
            result = _mm256_add_epi32(result, _mm256_or_si256(even, odd));
        }
        _mm256_storeu_si256(pixels, result);
    }
#elif defined(THORVG_NEON_SUPPORT)
    const auto opacityVec = vdupq_n_u16(opacity);
    const auto bias = vdupq_n_u16(510);
    const auto full = vdupq_n_u16(257);
    const auto lowByte = vdupq_n_u16(0x00ff);
    const auto colorEven = vreinterpretq_u16_u32(vdupq_n_u32(color & 0x00ff00ff));
    const auto colorOdd = vreinterpretq_u16_u32(vdupq_n_u32((color >> 8) & 0x00ff00ff));

    for (; len - x >= 4; x += 4) {
        auto pixels = dst + x;
        // copy A of each pixel into both 16-bit halves of its lane
        auto alpha = vshrq_n_u32(vld1q_u32(src + x), 24);
        alpha = vsliq_n_u32(alpha, alpha, 16);
        // ((opacity * alpha + 255) >> 8) + 1 = (alpha * opacity + 511) >> 8, rhadd adds without the 16-bit overflow
        auto factors = vshrq_n_u16(vrhaddq_u16(vmulq_u16(vreinterpretq_u16_u32(alpha), opacityVec), bias), 7);
        // sri keeps the high byte of the odd product and inserts the high byte of the even product below it
        auto result = vsriq_n_u16(vmulq_u16(colorOdd, factors), vmulq_u16(colorEven, factors), 8);
        if (direct) {
            // + ALPHA_BLEND(dst, 255 - a) with 256 - a = 257 - factors
            auto target = vreinterpretq_u16_u32(vld1q_u32(pixels));
            auto inverse = vsubq_u16(full, factors);
            auto even = vmulq_u16(vandq_u16(target, lowByte), inverse);
            auto odd = vmulq_u16(vshrq_n_u16(target, 8), inverse);
            result = vaddq_u16(result, vsriq_n_u16(odd, even, 8));
        }
        vst1q_u32(pixels, vreinterpretq_u32_u16(result));
    }
#endif
    for (; x < len; ++x) {
        auto a = MULTIPLY(opacity, A(src[x]));
        auto result = ALPHA_BLEND(color, a);
        if (direct) result += ALPHA_BLEND(dst[x], 255 - a);
        dst[x] = result;
    }
}
