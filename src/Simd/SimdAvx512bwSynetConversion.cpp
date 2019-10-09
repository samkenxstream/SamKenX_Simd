/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2019 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdConversion.h"
#include "Simd/SimdAvx2.h"
#include "Simd/SimdLog.h"

namespace Simd
{
#ifdef SIMD_AVX512BW_ENABLE    
    namespace Avx512bw
    {
        template <bool align> SIMD_INLINE void StoreScaled(float * ptr, __m512i value32, __m512 scale, __m512 shift)
        {
            Avx512f::Store<align>(ptr, _mm512_fmadd_ps(_mm512_cvtepi32_ps(value32), scale, shift));
        }

        const __m512i K16_BLUE_RED = SIMD_MM512_SET2_EPI16(Base::BLUE_TO_GRAY_WEIGHT, Base::RED_TO_GRAY_WEIGHT);
        const __m512i K16_GREEN_0000 = SIMD_MM512_SET2_EPI16(Base::GREEN_TO_GRAY_WEIGHT, 0x0000);
        const __m512i K32_ROUND_TERM = SIMD_MM512_SET1_EPI32(Base::BGR_TO_GRAY_ROUND_TERM);

        SIMD_INLINE __m512i BgraToGray32(__m512i bgra)
        {
            const __m512i g0a0 = _mm512_shuffle_epi8(bgra, K8_SUFFLE_BGRA_TO_G0A0);
            const __m512i b0r0 = _mm512_and_si512(bgra, K16_00FF);
            const __m512i weightedSum = _mm512_add_epi32(_mm512_madd_epi16(g0a0, K16_GREEN_0000), _mm512_madd_epi16(b0r0, K16_BLUE_RED));
            return _mm512_srli_epi32(_mm512_add_epi32(weightedSum, K32_ROUND_TERM), Base::BGR_TO_GRAY_AVERAGING_SHIFT);
        }

        template<SimdPixelFormatType format> SIMD_INLINE void SynetSetInput1(const uint8_t * src, __m512 scale, __m512 shift, float * dst);

        template<> SIMD_INLINE void SynetSetInput1<SimdPixelFormatGray8>(const uint8_t * src, __m512 scale, __m512 shift, float * dst)
        {
            StoreScaled<false>(dst + 0 * F, _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 0)), scale, shift);
            StoreScaled<false>(dst + 1 * F, _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 1)), scale, shift);
            StoreScaled<false>(dst + 2 * F, _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 2)), scale, shift);
            StoreScaled<false>(dst + 3 * F, _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 3)), scale, shift);
        }

        const __m512i K8_SHUFFLE_BGR_TO_BGRA = SIMD_MM512_SETR_EPI8(
            0x0, 0x1, 0x2, -1, 0x3, 0x4, 0x5, -1, 0x6, 0x7, 0x8, -1, 0x9, 0xA, 0xB, -1,
            0x0, 0x1, 0x2, -1, 0x3, 0x4, 0x5, -1, 0x6, 0x7, 0x8, -1, 0x9, 0xA, 0xB, -1,
            0x0, 0x1, 0x2, -1, 0x3, 0x4, 0x5, -1, 0x6, 0x7, 0x8, -1, 0x9, 0xA, 0xB, -1,
            0x0, 0x1, 0x2, -1, 0x3, 0x4, 0x5, -1, 0x6, 0x7, 0x8, -1, 0x9, 0xA, 0xB, -1);

        template<> SIMD_INLINE void SynetSetInput1<SimdPixelFormatBgr24>(const uint8_t * src, __m512 scale, __m512 shift, float * dst)
        {
            __m512i bgr0 = Load<false>(src + 0 * A);
            __m512i bgr1 = Load<false>(src + 1 * A);
            __m512i bgr2 = Load<false>(src + 2 * A);
            const __m512i bgra0 = _mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_0, bgr0);
            const __m512i bgra1 = _mm512_permutex2var_epi32(bgr0, K32_PERMUTE_BGR_TO_BGRA_1, bgr1);
            const __m512i bgra2 = _mm512_permutex2var_epi32(bgr1, K32_PERMUTE_BGR_TO_BGRA_2, bgr2);
            const __m512i bgra3 = _mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_3, bgr2);
            StoreScaled<false>(dst + 0 * F, BgraToGray32(_mm512_shuffle_epi8(bgra0, K8_SHUFFLE_BGR_TO_BGRA)), scale, shift);
            StoreScaled<false>(dst + 1 * F, BgraToGray32(_mm512_shuffle_epi8(bgra1, K8_SHUFFLE_BGR_TO_BGRA)), scale, shift);
            StoreScaled<false>(dst + 2 * F, BgraToGray32(_mm512_shuffle_epi8(bgra2, K8_SHUFFLE_BGR_TO_BGRA)), scale, shift);
            StoreScaled<false>(dst + 3 * F, BgraToGray32(_mm512_shuffle_epi8(bgra3, K8_SHUFFLE_BGR_TO_BGRA)), scale, shift);
        }

        template<> SIMD_INLINE void SynetSetInput1<SimdPixelFormatBgra32>(const uint8_t * src, __m512 scale, __m512 shift, float * dst)
        {
            StoreScaled<false>(dst + 0 * F, BgraToGray32(Load<false>((__m512i*)src + 0)), scale, shift);
            StoreScaled<false>(dst + 1 * F, BgraToGray32(Load<false>((__m512i*)src + 1)), scale, shift);
            StoreScaled<false>(dst + 2 * F, BgraToGray32(Load<false>((__m512i*)src + 2)), scale, shift);
            StoreScaled<false>(dst + 3 * F, BgraToGray32(Load<false>((__m512i*)src + 3)), scale, shift);
        }

        const __m512i K8_SHUFFLE_RGB_TO_BGRA = SIMD_MM512_SETR_EPI8(
            0x2, 0x1, 0x0, -1, 0x5, 0x4, 0x3, -1, 0x8, 0x7, 0x6, -1, 0xB, 0xA, 0x9, -1,
            0x2, 0x1, 0x0, -1, 0x5, 0x4, 0x3, -1, 0x8, 0x7, 0x6, -1, 0xB, 0xA, 0x9, -1,
            0x2, 0x1, 0x0, -1, 0x5, 0x4, 0x3, -1, 0x8, 0x7, 0x6, -1, 0xB, 0xA, 0x9, -1,
            0x2, 0x1, 0x0, -1, 0x5, 0x4, 0x3, -1, 0x8, 0x7, 0x6, -1, 0xB, 0xA, 0x9, -1);

        template<> SIMD_INLINE void SynetSetInput1<SimdPixelFormatRgb24>(const uint8_t * src, __m512 scale, __m512 shift, float * dst)
        {
            __m512i bgr0 = Load<false>(src + 0 * A);
            __m512i bgr1 = Load<false>(src + 1 * A);
            __m512i bgr2 = Load<false>(src + 2 * A);
            const __m512i bgra0 = _mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_0, bgr0);
            const __m512i bgra1 = _mm512_permutex2var_epi32(bgr0, K32_PERMUTE_BGR_TO_BGRA_1, bgr1);
            const __m512i bgra2 = _mm512_permutex2var_epi32(bgr1, K32_PERMUTE_BGR_TO_BGRA_2, bgr2);
            const __m512i bgra3 = _mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_3, bgr2);
            StoreScaled<false>(dst + 0 * F, BgraToGray32(_mm512_shuffle_epi8(bgra0, K8_SHUFFLE_RGB_TO_BGRA)), scale, shift);
            StoreScaled<false>(dst + 1 * F, BgraToGray32(_mm512_shuffle_epi8(bgra1, K8_SHUFFLE_RGB_TO_BGRA)), scale, shift);
            StoreScaled<false>(dst + 2 * F, BgraToGray32(_mm512_shuffle_epi8(bgra2, K8_SHUFFLE_RGB_TO_BGRA)), scale, shift);
            StoreScaled<false>(dst + 3 * F, BgraToGray32(_mm512_shuffle_epi8(bgra3, K8_SHUFFLE_RGB_TO_BGRA)), scale, shift);
        }

        template<SimdPixelFormatType format, size_t step> void SynetSetInput1(const uint8_t * src, size_t width, size_t height, size_t stride, const float * scale, const float * shift, float * dst)
        {
            __m512 _scale = _mm512_set1_ps(scale[0]);
            __m512 _shift = _mm512_set1_ps(shift[0]);
            size_t aligned = AlignLo(width, A);
            for (size_t y = 0; y < height; ++y)
            {
                for (size_t x = 0; x < aligned; x += A)
                    SynetSetInput1<format>(src + step * x, _scale, _shift, dst + x);
                if(aligned < width)
                    SynetSetInput1<format>(src + step * (width - A), _scale, _shift, dst + width - A);
                src += stride;
                dst += width;
            }
        }

        template<SimdPixelFormatType format> SIMD_INLINE void SynetSetInputNchw3A(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel);

        template<> SIMD_INLINE void SynetSetInputNchw3A<SimdPixelFormatGray8>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            __m512i gray0 = _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 0));
            __m512i gray1 = _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 1));
            __m512i gray2 = _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 2));
            __m512i gray3 = _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 3));
            StoreScaled<false>(dst + 0 * F, gray0, scale[0], shift[0]);
            StoreScaled<false>(dst + 1 * F, gray1, scale[0], shift[0]);
            StoreScaled<false>(dst + 2 * F, gray2, scale[0], shift[0]);
            StoreScaled<false>(dst + 3 * F, gray3, scale[0], shift[0]);
            dst += channel;
            StoreScaled<false>(dst + 0 * F, gray0, scale[1], shift[1]);
            StoreScaled<false>(dst + 1 * F, gray1, scale[1], shift[1]);
            StoreScaled<false>(dst + 2 * F, gray2, scale[1], shift[1]);
            StoreScaled<false>(dst + 3 * F, gray3, scale[1], shift[1]);
            dst += channel;
            StoreScaled<false>(dst + 0 * F, gray0, scale[2], shift[2]);
            StoreScaled<false>(dst + 1 * F, gray1, scale[2], shift[2]);
            StoreScaled<false>(dst + 2 * F, gray2, scale[2], shift[2]);
            StoreScaled<false>(dst + 3 * F, gray3, scale[2], shift[2]);
        }

        const __m512i K8_SHUFFLE_BGR_TO_B32 = SIMD_MM512_SETR_EPI8(
            0x0, -1, -1, -1, 0x3, -1, -1, -1, 0x6, -1, -1, -1, 0x9, -1, -1, -1,
            0x0, -1, -1, -1, 0x3, -1, -1, -1, 0x6, -1, -1, -1, 0x9, -1, -1, -1,
            0x0, -1, -1, -1, 0x3, -1, -1, -1, 0x6, -1, -1, -1, 0x9, -1, -1, -1,
            0x0, -1, -1, -1, 0x3, -1, -1, -1, 0x6, -1, -1, -1, 0x9, -1, -1, -1);

        const __m512i K8_SHUFFLE_BGR_TO_G32 = SIMD_MM512_SETR_EPI8(
            0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xA, -1, -1, -1,
            0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xA, -1, -1, -1,
            0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xA, -1, -1, -1,
            0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xA, -1, -1, -1);

        const __m512i K8_SHUFFLE_BGR_TO_R32 = SIMD_MM512_SETR_EPI8(
            0x2, -1, -1, -1, 0x5, -1, -1, -1, 0x8, -1, -1, -1, 0xB, -1, -1, -1,
            0x2, -1, -1, -1, 0x5, -1, -1, -1, 0x8, -1, -1, -1, 0xB, -1, -1, -1,
            0x2, -1, -1, -1, 0x5, -1, -1, -1, 0x8, -1, -1, -1, 0xB, -1, -1, -1,
            0x2, -1, -1, -1, 0x5, -1, -1, -1, 0x8, -1, -1, -1, 0xB, -1, -1, -1);

        SIMD_INLINE void SynetSetInputNchw3Bgr(__m512i bgra, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            StoreScaled<false>(dst + 0 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGR_TO_B32), scale[0], shift[0]);
            StoreScaled<false>(dst + 1 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGR_TO_G32), scale[1], shift[1]);
            StoreScaled<false>(dst + 2 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGR_TO_R32), scale[2], shift[2]);
        }  

        const __m512i K32_PERMUTE_BGR_TO_BGRA_BEG = SIMD_MM512_SETR_EPI32(0x0, 0x1, 0x2, -1, 0x3, 0x4, 0x5, -1, 0x6, 0x7, 0x8, -1, 0x9, 0xA, 0xB, -1);
        const __m512i K32_PERMUTE_BGR_TO_BGRA_END = SIMD_MM512_SETR_EPI32(0x4, 0x5, 0x6, -1, 0x7, 0x8, 0x9, -1, 0xA, 0xB, 0xC, -1, 0xD, 0xE, 0xF, -1);

        template<> SIMD_INLINE void SynetSetInputNchw3A<SimdPixelFormatBgr24>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            SynetSetInputNchw3Bgr(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 0 * F)), scale, shift, dst + 0 * F, channel);
            SynetSetInputNchw3Bgr(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 3 * F)), scale, shift, dst + 1 * F, channel);
            SynetSetInputNchw3Bgr(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 6 * F)), scale, shift, dst + 2 * F, channel);
            SynetSetInputNchw3Bgr(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_END, Load<false>(src + 8 * F)), scale, shift, dst + 3 * F, channel);
        }

        const __m512i K8_SHUFFLE_BGRA_TO_B32 = SIMD_MM512_SETR_EPI8(
            0x0, -1, -1, -1, 0x4, -1, -1, -1, 0x8, -1, -1, -1, 0xC, -1, -1, -1,
            0x0, -1, -1, -1, 0x4, -1, -1, -1, 0x8, -1, -1, -1, 0xC, -1, -1, -1,
            0x0, -1, -1, -1, 0x4, -1, -1, -1, 0x8, -1, -1, -1, 0xC, -1, -1, -1,
            0x0, -1, -1, -1, 0x4, -1, -1, -1, 0x8, -1, -1, -1, 0xC, -1, -1, -1);

        const __m512i K8_SHUFFLE_BGRA_TO_G32 = SIMD_MM512_SETR_EPI8(
            0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xD, -1, -1, -1,
            0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xD, -1, -1, -1,
            0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xD, -1, -1, -1,
            0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xD, -1, -1, -1);

        const __m512i K8_SHUFFLE_BGRA_TO_R32 = SIMD_MM512_SETR_EPI8(
            0x2, -1, -1, -1, 0x6, -1, -1, -1, 0xA, -1, -1, -1, 0xE, -1, -1, -1,
            0x2, -1, -1, -1, 0x6, -1, -1, -1, 0xA, -1, -1, -1, 0xE, -1, -1, -1,
            0x2, -1, -1, -1, 0x6, -1, -1, -1, 0xA, -1, -1, -1, 0xE, -1, -1, -1,
            0x2, -1, -1, -1, 0x6, -1, -1, -1, 0xA, -1, -1, -1, 0xE, -1, -1, -1);

        SIMD_INLINE void SynetSetInputNchw3Bgra(__m512i bgra, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            StoreScaled<false>(dst + 0 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGRA_TO_B32), scale[0], shift[0]);
            StoreScaled<false>(dst + 1 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGRA_TO_G32), scale[1], shift[1]);
            StoreScaled<false>(dst + 2 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGRA_TO_R32), scale[2], shift[2]);
        }

        template<> SIMD_INLINE void SynetSetInputNchw3A<SimdPixelFormatBgra32>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            SynetSetInputNchw3Bgra(Load<false>(src + 0 * A), scale, shift, dst + 0 * F, channel);
            SynetSetInputNchw3Bgra(Load<false>(src + 1 * A), scale, shift, dst + 1 * F, channel);
            SynetSetInputNchw3Bgra(Load<false>(src + 2 * A), scale, shift, dst + 2 * F, channel);
            SynetSetInputNchw3Bgra(Load<false>(src + 3 * A), scale, shift, dst + 3 * F, channel);
        }

        SIMD_INLINE void SynetSetInputNchw3Rgb(__m512i bgra, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            StoreScaled<false>(dst + 0 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGR_TO_R32), scale[0], shift[0]);
            StoreScaled<false>(dst + 1 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGR_TO_G32), scale[1], shift[1]);
            StoreScaled<false>(dst + 2 * channel, _mm512_shuffle_epi8(bgra, K8_SHUFFLE_BGR_TO_B32), scale[2], shift[2]);
        }

        template<> SIMD_INLINE void SynetSetInputNchw3A<SimdPixelFormatRgb24>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            SynetSetInputNchw3Rgb(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 0 * F)), scale, shift, dst + 0 * F, channel);
            SynetSetInputNchw3Rgb(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 3 * F)), scale, shift, dst + 1 * F, channel);
            SynetSetInputNchw3Rgb(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 6 * F)), scale, shift, dst + 2 * F, channel);
            SynetSetInputNchw3Rgb(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_END, Load<false>(src + 8 * F)), scale, shift, dst + 3 * F, channel);
        }

        template<SimdPixelFormatType format> SIMD_INLINE void SynetSetInputNchw3F(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel);

        template<> SIMD_INLINE void SynetSetInputNchw3F<SimdPixelFormatGray8>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            __m512i gray = _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src));
            StoreScaled<false>(dst + 0 * channel, gray, scale[0], shift[0]);
            StoreScaled<false>(dst + 1 * channel, gray, scale[1], shift[1]);
            StoreScaled<false>(dst + 2 * channel, gray, scale[2], shift[2]);
        }

        template<> SIMD_INLINE void SynetSetInputNchw3F<SimdPixelFormatBgr24>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            SynetSetInputNchw3Bgr(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_END, Load<false>(src - F)), scale, shift, dst, channel);
        }

        template<> SIMD_INLINE void SynetSetInputNchw3F<SimdPixelFormatBgra32>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            SynetSetInputNchw3Bgra(Load<false>(src), scale, shift, dst, channel);
        }

        template<> SIMD_INLINE void SynetSetInputNchw3F<SimdPixelFormatRgb24>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst, size_t channel)
        {
            SynetSetInputNchw3Rgb(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_END, Load<false>(src - F)), scale, shift, dst, channel);
        }

        template<SimdPixelFormatType format, size_t step> void SynetSetInputNchw3(const uint8_t * src, size_t width, size_t height, size_t stride, const float * scale, const float * shift, float * dst)
        {
            size_t widthF = AlignLo(width, F), widthA = AlignLo(width, A), channel = width * height;
            __m512 _scale[3], _shift[3];
            for (size_t i = 0; i < 3; ++i)
            {
                _scale[i] = _mm512_set1_ps(scale[i]);
                _shift[i] = _mm512_set1_ps(shift[i]);
            }
            for (size_t y = 0; y < height; ++y)
            {
                size_t x = 0;
                for (; x < widthA; x += A)
                    SynetSetInputNchw3A<format>(src + step * x, _scale, _shift, dst + x, channel);
                for (; x < widthF; x += F)
                    SynetSetInputNchw3F<format>(src + step * x, _scale, _shift, dst + x, channel);
                if (widthF < width)
                    SynetSetInputNchw3F<format>(src + step * (width - F), _scale, _shift, dst + width - F, channel);
                src += stride;
                dst += width;
            }
        }

        template<SimdPixelFormatType format> SIMD_INLINE void SynetSetInputNhwc3A(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst);

        template<> SIMD_INLINE void SynetSetInputNhwc3A<SimdPixelFormatGray8>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst)
        {
            __m128i gray0 = Sse2::Load<false>((__m128i*)src + 0);
            StoreScaled<false>(dst + 0x0 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray0, Ssse3::K8_SHUFFLE_GRAY_TO_BGR0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x1 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray0, Ssse3::K8_SHUFFLE_GRAY_TO_BGR1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x2 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray0, Ssse3::K8_SHUFFLE_GRAY_TO_BGR2)), scale[2], shift[2]);
            __m128i gray1 = Sse2::Load<false>((__m128i*)src + 1);
            StoreScaled<false>(dst + 0x3 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray1, Ssse3::K8_SHUFFLE_GRAY_TO_BGR0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x4 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray1, Ssse3::K8_SHUFFLE_GRAY_TO_BGR1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x5 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray1, Ssse3::K8_SHUFFLE_GRAY_TO_BGR2)), scale[2], shift[2]);
            __m128i gray2 = Sse2::Load<false>((__m128i*)src + 2);
            StoreScaled<false>(dst + 0x6 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray2, Ssse3::K8_SHUFFLE_GRAY_TO_BGR0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x7 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray2, Ssse3::K8_SHUFFLE_GRAY_TO_BGR1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x8 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray2, Ssse3::K8_SHUFFLE_GRAY_TO_BGR2)), scale[2], shift[2]);
            __m128i gray3 = Sse2::Load<false>((__m128i*)src + 3);
            StoreScaled<false>(dst + 0x9 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray3, Ssse3::K8_SHUFFLE_GRAY_TO_BGR0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0xA * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray3, Ssse3::K8_SHUFFLE_GRAY_TO_BGR1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0xB * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray3, Ssse3::K8_SHUFFLE_GRAY_TO_BGR2)), scale[2], shift[2]);
        }

        template<> SIMD_INLINE void SynetSetInputNhwc3A<SimdPixelFormatBgr24>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst)
        {
            __m512i src0 = Load<false>((__m512i*)src + 0);
            StoreScaled<false>(dst + 0x0 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src0, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x1 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src0, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x2 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src0, 2)), scale[2], shift[2]);
            StoreScaled<false>(dst + 0x3 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src0, 3)), scale[0], shift[0]);
            __m512i src1 = Load<false>((__m512i*)src + 1);
            StoreScaled<false>(dst + 0x4 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src1, 0)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x5 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src1, 1)), scale[2], shift[2]);
            StoreScaled<false>(dst + 0x6 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src1, 2)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x7 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src1, 3)), scale[1], shift[1]);
            __m512i src2 = Load<false>((__m512i*)src + 2);
            StoreScaled<false>(dst + 0x8 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src2, 0)), scale[2], shift[2]);
            StoreScaled<false>(dst + 0x9 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src2, 1)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0xA * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src2, 2)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0xB * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(src2, 3)), scale[2], shift[2]);
        }

        const __m512i K8_SUFFLE_BGRA_TO_BGR = SIMD_MM512_SETR_EPI8(
            0x0, 0x1, 0x2, 0x4, 0x5, 0x6, 0x8, 0x9, 0xA, 0xC, 0xD, 0xE, -1, -1, -1, -1,
            0x0, 0x1, 0x2, 0x4, 0x5, 0x6, 0x8, 0x9, 0xA, 0xC, 0xD, 0xE, -1, -1, -1, -1,
            0x0, 0x1, 0x2, 0x4, 0x5, 0x6, 0x8, 0x9, 0xA, 0xC, 0xD, 0xE, -1, -1, -1, -1,
            0x0, 0x1, 0x2, 0x4, 0x5, 0x6, 0x8, 0x9, 0xA, 0xC, 0xD, 0xE, -1, -1, -1, -1);

        const __m512i K32_PERMUTE_BGRA_TO_BGR = SIMD_MM512_SETR_EPI32(0x0, 0x1, 0x2, 0x4, 0x5, 0x6, 0x8, 0x9, 0xA, 0xC, 0xD, 0xE, -1, -1, -1, -1);

        template<> SIMD_INLINE void SynetSetInputNhwc3A<SimdPixelFormatBgra32>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst)
        {
            __m512i bgr0 = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(Load<false>(src + 0 * A), K8_SUFFLE_BGRA_TO_BGR));
            StoreScaled<false>(dst + 0x0 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr0, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x1 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr0, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x2 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr0, 2)), scale[2], shift[2]);
            __m512i bgr1 = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(Load<false>(src + 1 * A), K8_SUFFLE_BGRA_TO_BGR));
            StoreScaled<false>(dst + 0x3 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr1, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x4 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr1, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x5 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr1, 2)), scale[2], shift[2]);
            __m512i bgr2 = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(Load<false>(src + 2 * A), K8_SUFFLE_BGRA_TO_BGR));
            StoreScaled<false>(dst + 0x6 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr2, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x7 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr2, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x8 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr2, 2)), scale[2], shift[2]);
            __m512i bgr3 = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(Load<false>(src + 3 * A), K8_SUFFLE_BGRA_TO_BGR));
            StoreScaled<false>(dst + 0x9 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr3, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0xA * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr3, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0xB * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr3, 2)), scale[2], shift[2]);
        }

        const __m512i K8_SUFFLE_RGB_TO_BGR = SIMD_MM512_SETR_EPI8(
            0x2, 0x1, 0x0, 0x5, 0x4, 0x3, 0x8, 0x7, 0x6, 0xB, 0xA, 0x9, -1, -1, -1, -1,
            0x2, 0x1, 0x0, 0x5, 0x4, 0x3, 0x8, 0x7, 0x6, 0xB, 0xA, 0x9, -1, -1, -1, -1,
            0x2, 0x1, 0x0, 0x5, 0x4, 0x3, 0x8, 0x7, 0x6, 0xB, 0xA, 0x9, -1, -1, -1, -1,
            0x2, 0x1, 0x0, 0x5, 0x4, 0x3, 0x8, 0x7, 0x6, 0xB, 0xA, 0x9, -1, -1, -1, -1);

        template<> SIMD_INLINE void SynetSetInputNhwc3A<SimdPixelFormatRgb24>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst)
        {
            __m512i bgr0 = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 0*F)), K8_SUFFLE_RGB_TO_BGR));
            StoreScaled<false>(dst + 0x0 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr0, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x1 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr0, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x2 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr0, 2)), scale[2], shift[2]);
            __m512i bgr1 = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 3*F)), K8_SUFFLE_RGB_TO_BGR));
            StoreScaled<false>(dst + 0x3 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr1, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x4 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr1, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x5 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr1, 2)), scale[2], shift[2]);
            __m512i bgr2 = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_BEG, Load<false>(src + 6*F)), K8_SUFFLE_RGB_TO_BGR));
            StoreScaled<false>(dst + 0x6 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr2, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x7 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr2, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x8 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr2, 2)), scale[2], shift[2]);
            __m512i bgr3 = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_END, Load<false>(src + 8*F)), K8_SUFFLE_RGB_TO_BGR));
            StoreScaled<false>(dst + 0x9 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr3, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0xA * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr3, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0xB * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr3, 2)), scale[2], shift[2]);
        }

        template<SimdPixelFormatType format> SIMD_INLINE void SynetSetInputNhwc3F(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst);

        template<> SIMD_INLINE void SynetSetInputNhwc3F<SimdPixelFormatGray8>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst)
        {
            __m128i gray0 = Sse2::Load<false>((__m128i*)src + 0);
            StoreScaled<false>(dst + 0x0 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray0, Ssse3::K8_SHUFFLE_GRAY_TO_BGR0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x1 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray0, Ssse3::K8_SHUFFLE_GRAY_TO_BGR1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x2 * F, _mm512_cvtepu8_epi32(_mm_shuffle_epi8(gray0, Ssse3::K8_SHUFFLE_GRAY_TO_BGR2)), scale[2], shift[2]);
        }

        template<> SIMD_INLINE void SynetSetInputNhwc3F<SimdPixelFormatBgr24>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst)
        {
            StoreScaled<false>(dst + 0x0 * F, _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 0x0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x1 * F, _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 0x1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x2 * F, _mm512_cvtepu8_epi32(Sse2::Load<false>((__m128i*)src + 0x2)), scale[2], shift[2]);
        }

        template<> SIMD_INLINE void SynetSetInputNhwc3F<SimdPixelFormatBgra32>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst)
        {
            __m512i bgr = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(Load<false>(src), K8_SUFFLE_BGRA_TO_BGR));
            StoreScaled<false>(dst + 0x0 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x1 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x2 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr, 2)), scale[2], shift[2]);
        }

        template<> SIMD_INLINE void SynetSetInputNhwc3F<SimdPixelFormatRgb24>(const uint8_t * src, const __m512 * scale, const __m512 * shift, float * dst)
        {
            __m512i bgr = _mm512_permutexvar_epi32(K32_PERMUTE_BGRA_TO_BGR, _mm512_shuffle_epi8(_mm512_permutexvar_epi32(K32_PERMUTE_BGR_TO_BGRA_END, Load<false>(src - F)), K8_SUFFLE_RGB_TO_BGR));
            StoreScaled<false>(dst + 0x0 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr, 0)), scale[0], shift[0]);
            StoreScaled<false>(dst + 0x1 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr, 1)), scale[1], shift[1]);
            StoreScaled<false>(dst + 0x2 * F, _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(bgr, 2)), scale[2], shift[2]);
        }

        template<SimdPixelFormatType format, size_t step> void SynetSetInputNhwc3(const uint8_t * src, size_t width, size_t height, size_t stride, const float * scale, const float * shift, float * dst)
        {
            size_t widthF = AlignLo(width, F);
            size_t widthA = AlignLo(width, A);
            __m512 _scale[3], _shift[3];
            for (float *sc = (float*)_scale, *sh = (float*)_shift, *end = sc + 48; sc < end; sc += 3, sh += 3)
            {
                sc[0] = scale[0]; sc[1] = scale[1]; sc[2] = scale[2]; 
                sh[0] = shift[0]; sh[1] = shift[1]; sh[2] = shift[2];
            }
            for (size_t y = 0; y < height; ++y)
            {
                size_t x = 0;
                for (; x < widthA; x += A)
                    SynetSetInputNhwc3A<format>(src + step * x, _scale, _shift, dst + 3 * x);
                for (; x < widthF; x += F)
                    SynetSetInputNhwc3F<format>(src + step * x, _scale, _shift, dst + 3 * x);
                if (widthF < width)
                    SynetSetInputNhwc3F<format>(src + step * (width - F), _scale, _shift, dst + 3 * (width - F));
                src += stride;
                dst += 3*width;
            }
        }

        void SynetSetInput(const uint8_t * src, size_t width, size_t height, size_t stride, SimdPixelFormatType srcFormat,
            const float * lower, const float * upper, float * dst, size_t channels, SimdTensorFormatType dstFormat)
        {
            assert(width >= A);

            float scale[3];
            for (size_t i = 0; i < channels; ++i)
                scale[i] = (upper[i] - lower[i]) / 255.0f;
            switch (channels)
            {
            case 1:
                switch (srcFormat)
                {
                case SimdPixelFormatGray8: SynetSetInput1<SimdPixelFormatGray8, 1>(src, width, height, stride, scale, lower, dst); return;
                case SimdPixelFormatBgr24: SynetSetInput1<SimdPixelFormatBgr24, 3>(src, width, height, stride, scale, lower, dst); return;
                case SimdPixelFormatBgra32: SynetSetInput1<SimdPixelFormatBgra32, 4>(src, width, height, stride, scale, lower, dst); return;
                case SimdPixelFormatRgb24: SynetSetInput1<SimdPixelFormatRgb24, 3>(src, width, height, stride, scale, lower, dst); return;
                default: assert(0);
                }
                break;
            case 3:
                switch (dstFormat)
                {
                case SimdTensorFormatNchw:
                    switch (srcFormat)
                    {
                    case SimdPixelFormatGray8: SynetSetInputNchw3<SimdPixelFormatGray8, 1>(src, width, height, stride, scale, lower, dst); return;
                    case SimdPixelFormatBgr24: SynetSetInputNchw3<SimdPixelFormatBgr24, 3>(src, width, height, stride, scale, lower, dst); return;
                    case SimdPixelFormatBgra32: SynetSetInputNchw3<SimdPixelFormatBgra32, 4>(src, width, height, stride, scale, lower, dst); return;
                    case SimdPixelFormatRgb24: SynetSetInputNchw3<SimdPixelFormatRgb24, 3>(src, width, height, stride, scale, lower, dst); return;
                    default: assert(0);
                    }
                    break;
                case SimdTensorFormatNhwc:
                    switch (srcFormat)
                    {
                    case SimdPixelFormatGray8: SynetSetInputNhwc3<SimdPixelFormatGray8, 1>(src, width, height, stride, scale, lower, dst); return;
                    case SimdPixelFormatBgr24: SynetSetInputNhwc3<SimdPixelFormatBgr24, 3>(src, width, height, stride, scale, lower, dst); return;
                    case SimdPixelFormatBgra32: SynetSetInputNhwc3<SimdPixelFormatBgra32, 4>(src, width, height, stride, scale, lower, dst); return;
                    case SimdPixelFormatRgb24: SynetSetInputNhwc3<SimdPixelFormatRgb24, 3>(src, width, height, stride, scale, lower, dst); return;
                    default: assert(0);
                    }
                    break;
                default: assert(0);
                }
            default: assert(0);
            }
        }
    }
#endif//SIMD_AVX512BW_ENABLE
}
