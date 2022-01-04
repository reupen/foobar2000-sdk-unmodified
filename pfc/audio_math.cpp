#include "pfc-lite.h"
#include "audio_sample.h"
#include "primitives.h"


#if (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__)
#define AUDIO_MATH_SSE
#include <xmmintrin.h>

#ifndef _mm_loadu_si32
#define _mm_loadu_si32(p) _mm_cvtsi32_si128(*(unsigned int const*)(p))
#endif
#ifndef _mm_storeu_si32
#define _mm_storeu_si32(p, a) (void)(*(int*)(p) = _mm_cvtsi128_si32((a)))
#endif

#endif

#if defined( __aarch64__ ) || defined( __ARM_NEON__ )
#define AUDIO_MATH_NEON
#include <arm_neon.h>
#endif

[[maybe_unused]] inline static audio_sample noopt_calculate_peak(const audio_sample * p_src,t_size p_num)
{
	audio_sample peak = 0;
	t_size num = p_num;
	for(;num;num--)
	{
		audio_sample temp = (audio_sample)fabs(*(p_src++));
		if (temp>peak) peak = temp;
	}
	return peak;
}




[[maybe_unused]] inline static void noopt_convert_to_32bit(const audio_sample * p_source,t_size p_count,t_int32 * p_output,float p_scale)
{
	t_size num = p_count;
	for(;num;--num)
	{
		t_int64 val = pfc::audio_math::rint64( *(p_source++) * p_scale );
		if (val < INT32_MIN) val = INT32_MIN;
		else if (val > INT32_MAX) val = INT32_MAX;
		*(p_output++) = (t_int32) val;
	}
}

inline static void noopt_convert_to_16bit(const audio_sample * p_source,t_size p_count,t_int16 * p_output,float p_scale) {
	for(t_size n=0;n<p_count;n++) {
		*(p_output++) = (t_int16) pfc::clip_t<int32_t>(pfc::audio_math::rint32(*(p_source++)*p_scale),INT16_MIN,INT16_MAX);
	}
}

[[maybe_unused]] inline static void noopt_convert_from_int16(const t_int16 * __restrict p_source,t_size p_count, audio_sample * __restrict p_output,float p_scale)
{
	t_size num = p_count;
	for(;num;num--)
		*(p_output++) = (audio_sample)*(p_source++) * p_scale;
}



[[maybe_unused]] inline static void noopt_convert_from_int32(const t_int32 * __restrict p_source,t_size p_count,audio_sample * __restrict p_output,float p_scale)
{
	t_size num = p_count;
	for(;num;num--)
		*(p_output++) = (audio_sample)*(p_source++) * p_scale;
}

[[maybe_unused]] inline static void noopt_scale(const audio_sample * p_source,t_size p_count,audio_sample * p_output,audio_sample p_scale)
{
	for(t_size n=0;n<p_count;n++)
		p_output[n] = p_source[n] * p_scale;
}

#ifdef AUDIO_MATH_NEON

#ifdef __aarch64__
#define _vmaxvq_f32_wrap vmaxvq_f32
#else
inline float _vmaxvq_f32_wrap( float32x4_t arg ) {
    return pfc::max_t<float>( pfc::max_t<float>(arg[0], arg[1]), pfc::max_t<float>(arg[2], arg[3]) );
}
#endif

inline static audio_sample neon_calculate_peak( const audio_sample * p_source, size_t p_count ) {
    size_t num = p_count / 8;
    float32x4_t ret1 = {}, ret2 = {};
    for(;num;--num) {
        float32x4_t f32lo = vld1q_f32( p_source );
        float32x4_t f32hi = vld1q_f32( p_source + 4 );
        p_source += 8;
        ret1 = vmaxq_f32(ret1, vabsq_f32(f32lo));
        ret2 = vmaxq_f32(ret2, vabsq_f32(f32hi));
    }
    
    audio_sample ret = _vmaxvq_f32_wrap(vmaxq_f32( ret1, ret2 ));
    
    size_t rem = p_count % 8;
    if ( rem != 0 ) {
        audio_sample v = noopt_calculate_peak( p_source, p_count % 8);
        if (v > ret) ret = v;
    }

    return ret;
}

inline static void neon_scale(const audio_sample * p_source,size_t p_count, audio_sample * p_output,float p_scale) {
    size_t num = p_count / 8;
    for(;num;--num) {
        float32x4_t lo = vld1q_f32( p_source );
        float32x4_t hi = vld1q_f32( p_source + 4 );

        lo = vmulq_n_f32( lo, p_scale);
        hi = vmulq_n_f32( hi, p_scale);
        
        vst1q_f32( p_output, lo );
        vst1q_f32( p_output+4, hi );
        
        p_source += 8;
        p_output += 8;
    }
    
    noopt_scale( p_source, p_count % 8, p_output, p_scale);
}
inline static void neon_convert_to_int32(const audio_sample * __restrict p_source,t_size p_count, int32_t * __restrict p_output,float p_scale)
{
    size_t num = p_count / 8;
    for(;num;--num) {
        float32x4_t f32lo = vld1q_f32( p_source );
        float32x4_t f32hi = vld1q_f32( p_source + 4 );
        
        int32x4_t lo = vcvtq_s32_f32( vmulq_n_f32(f32lo, p_scale) );
        int32x4_t hi = vcvtq_s32_f32( vmulq_n_f32(f32hi, p_scale) );
        
        vst1q_s32(p_output, lo);
        vst1q_s32(p_output+4, hi);
        
        p_source += 8;
        p_output += 8;
        
    }
    
    noopt_convert_to_32bit(p_source, p_count % 8, p_output, p_scale);
}

inline static void neon_convert_from_int32(const int32_t * __restrict p_source,t_size p_count, audio_sample * __restrict p_output,float p_scale)
{
    size_t num = p_count / 8;
    size_t rem = p_count % 8;
    for(;num;num--) {
        int32x4_t i32lo = vld1q_s32( p_source );
        int32x4_t i32hi = vld1q_s32( p_source + 4 );
        float32x4_t f32vl = vcvtq_f32_s32(i32lo);
        float32x4_t f32vh = vcvtq_f32_s32(i32hi);

        vst1q_f32(&p_output[0], vmulq_n_f32(f32vl, p_scale));
        vst1q_f32(&p_output[4], vmulq_n_f32(f32vh, p_scale));
        
        p_source += 8;
        p_output += 8;
        
    }
    
    noopt_convert_from_int32( p_source, rem, p_output, p_scale );
}

inline static void neon_convert_to_int16(const audio_sample * __restrict p_source,t_size p_count, int16_t * __restrict p_output,float p_scale)
{
    size_t num = p_count / 8;
    size_t rem = p_count % 8;
    for(;num;--num) {
        float32x4_t f32lo = vld1q_f32( p_source );
        float32x4_t f32hi = vld1q_f32( p_source + 4);
        
        int32x4_t lo = vcvtq_s32_f32( vmulq_n_f32(f32lo, p_scale) );
        int32x4_t hi = vcvtq_s32_f32( vmulq_n_f32(f32hi, p_scale) );
        
        vst1q_s16(&p_output[0], vcombine_s16( vqmovn_s32( lo ), vqmovn_s32( hi ) ) );
        
        p_source += 8;
        p_output += 8;
        
    }
    
    noopt_convert_to_16bit(p_source, rem, p_output, p_scale);
    
}
inline static void neon_convert_from_int16(const t_int16 * __restrict p_source,t_size p_count, audio_sample * __restrict p_output,float p_scale)
{
    size_t num = p_count / 8;
    size_t rem = p_count % 8;
    for(;num;num--) {
        auto i16lo = vld1_s16(p_source);
        auto i16hi = vld1_s16(p_source + 4);

        float32x4_t f32vl = vcvtq_f32_s32(vmovl_s16 (i16lo));
        float32x4_t f32vh = vcvtq_f32_s32(vmovl_s16 (i16hi));

        vst1q_f32(&p_output[0], vmulq_n_f32(f32vl, p_scale));
        vst1q_f32(&p_output[4], vmulq_n_f32(f32vh, p_scale));

        p_source += 8;
        p_output += 8;

    }
    
    noopt_convert_from_int16( p_source, rem, p_output, p_scale );
}
#endif

#ifdef AUDIO_MATH_SSE

inline void convert_to_32bit_sse2(const audio_sample* p_src, size_t numTotal, t_int32* p_dst, float p_mul)
{

    // Implementation notes
    // There doesn't seem to be a nice and tidy way to convert float to int32 with graceful clipping to INT32_MIN .. INT32_MAX range.
    // While low clipping at INT32_MIN can be accomplished with _mm_max_ps(), high clipping needs float compare THEN substitute bad int with INT32_MAX.
    // The best we could do with _mm_min_ps() would result with high clipping at 0x7FFFFF80 instead of 0x7FFFFFFF (INT32_MAX).
    // We store masks from float compare and fix ints according to the mask later.

    __m128 mul = _mm_set1_ps(p_mul);
    __m128 loF = _mm_set1_ps((float)INT32_MIN);
    __m128 hiF = _mm_set1_ps((float)INT32_MAX);
    // __m128i loI = _mm_set1_epi32(INT32_MIN);
    __m128i hiI = _mm_set1_epi32(INT32_MAX);

    size_t num = numTotal / 4;
    size_t rem = numTotal % 4;
    for (; num; --num) {
        __m128 s = _mm_mul_ps(mul, _mm_loadu_ps(p_src)); p_src += 4;

        s = _mm_max_ps(s, loF);

        // __m128i maskLo = _mm_castps_si128(_mm_cmple_ps(s, loF));
        __m128i maskHi = _mm_castps_si128(_mm_cmpge_ps(s, hiF));

        __m128i i = _mm_cvtps_epi32(s);

        // i = _mm_or_si128(_mm_andnot_si128(maskLo, i), _mm_and_si128(loI, maskLo));
        i = _mm_or_si128(_mm_andnot_si128(maskHi, i), _mm_and_si128(hiI, maskHi));

        _mm_storeu_si128((__m128i*) p_dst, i); p_dst += 4;
    }

    for (; rem; --rem) {
        __m128 s = _mm_mul_ss(_mm_load_ss(p_src++), mul);
        s = _mm_max_ss(s, loF);

        // __m128i maskLo = _mm_castps_si128( _mm_cmple_ss(s, loF) );
        __m128i maskHi = _mm_castps_si128(_mm_cmpge_ss(s, hiF));

        __m128i i = _mm_cvtps_epi32(s); // not ss

        // i = _mm_or_si128(_mm_andnot_si128(maskLo, i), _mm_and_si128(loI, maskLo));
        i = _mm_or_si128(_mm_andnot_si128(maskHi, i), _mm_and_si128(hiI, maskHi));

        _mm_storeu_si32(p_dst++, i);
    }
}

inline void convert_from_int16_sse2(const t_int16 * p_source,t_size p_count,audio_sample * p_output,float p_scale)
{
    while(!pfc::is_ptr_aligned_t<16>(p_output) && p_count) {
        *(p_output++) = (audio_sample)*(p_source++) * p_scale;
        p_count--;
    }

    {
        __m128 mul = _mm_set1_ps(p_scale);
        __m128i nulls = _mm_setzero_si128();
        __m128i delta1 = _mm_set1_epi16((int16_t)0x8000);
        __m128i delta2 = _mm_set1_epi32((int32_t)0x8000);

        for(t_size loop = p_count >> 3;loop;--loop) {
            __m128i source, temp1, temp2; __m128 float1, float2;
            source = _mm_loadu_si128((__m128i*)p_source);
            source = _mm_xor_si128(source,delta1);
            temp1 = _mm_unpacklo_epi16(source,nulls);
            temp2 = _mm_unpackhi_epi16(source,nulls);
            temp1 = _mm_sub_epi32(temp1,delta2);
            temp2 = _mm_sub_epi32(temp2,delta2);
            p_source += 8;
            float1 = _mm_cvtepi32_ps(temp1);
            float2 = _mm_cvtepi32_ps(temp2);
            float1 = _mm_mul_ps(float1,mul);
            float2 = _mm_mul_ps(float2,mul);
            _mm_storeu_ps(p_output,float1);
            _mm_storeu_ps(p_output+4,float2);
            p_output += 8;
        }
        
        p_count &= 7;
    }

    while(p_count) {
        *(p_output++) = (audio_sample)*(p_source++) * p_scale;
        p_count--;
    }
}
inline static void convert_to_16bit_sse2(const audio_sample * p_source,t_size p_count,t_int16 * p_output,float p_scale)
{
    size_t num = p_count/8;
    size_t rem = p_count%8;
    __m128 mul = _mm_load1_ps(&p_scale);
    for(;num;--num)
    {
        __m128 temp1,temp2; __m128i itemp1, itemp2;
        temp1 = _mm_loadu_ps(p_source);
        temp2 = _mm_loadu_ps(p_source+4);
        temp1 = _mm_mul_ps(temp1,mul);
        temp2 = _mm_mul_ps(temp2,mul);
        p_source += 8;
        itemp1 = _mm_cvtps_epi32(temp1);
        itemp2 = _mm_cvtps_epi32(temp2);
        _mm_storeu_si128( (__m128i*)p_output, _mm_packs_epi32(itemp1, itemp2) );
        p_output += 8;
    }
    
    noopt_convert_to_16bit(p_source, rem, p_output, p_scale);
}

inline audio_sample sse_calculate_peak( const audio_sample * src, size_t count ) {
    size_t num = count/8;
    size_t rem = count%8;
        
    __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    __m128 acc1 = _mm_setzero_ps(), acc2 = _mm_setzero_ps();
    
    for(;num;--num) {
        __m128 v1 = _mm_loadu_ps( src );
        __m128 v2 = _mm_loadu_ps( src + 4 );
        v1 = _mm_and_ps( v1, mask );
        v2 = _mm_and_ps( v2, mask );
        // Two acc channels so one _mm_max_ps doesn't block the other
        acc1 = _mm_max_ps( acc1, v1 );
        acc2 = _mm_max_ps( acc2, v2 );
        src += 8;
    }
    
    float ret;
    {
        float blah[4];
        _mm_storeu_ps(blah, _mm_max_ps( acc1, acc2 ));
        __m128 acc = _mm_load_ss( &blah[0] );
        acc = _mm_max_ss( acc, _mm_load_ss( &blah[1] ) );
        acc = _mm_max_ss( acc, _mm_load_ss( &blah[2] ) );
        acc = _mm_max_ss( acc, _mm_load_ss( &blah[3] ) );
        ret = _mm_cvtss_f32(acc);
    }
    if ( rem > 0 ) {
        __m128 acc = _mm_set_ss( ret );
        for( ;rem; --rem) {
            __m128 v = _mm_load_ss( src++ );
            v = _mm_and_ps( v, mask );
            acc = _mm_max_ss( acc, v );
        }
        ret = _mm_cvtss_f32(acc);
    }
    return ret;
}

inline void sse_convert_from_int32(const int32_t* source, size_t count, audio_sample* output, audio_sample scale) {
    __m128 mul = _mm_set1_ps(scale);
    for (size_t num = count/8; num; --num)
    {
        __m128i itemp1, itemp2; __m128 temp1, temp2;
        itemp1 = _mm_loadu_si128((__m128i*)source);
        itemp2 = _mm_loadu_si128((__m128i*)source + 1);
        temp1 = _mm_cvtepi32_ps(itemp1);
        temp2 = _mm_cvtepi32_ps(itemp2);
        source += 8;
        temp1 = _mm_mul_ps(temp1, mul);
        temp2 = _mm_mul_ps(temp2, mul);
        _mm_storeu_ps(output, temp1);
        _mm_storeu_ps(output + 4, temp2);
        output += 8;
    }
    for (size_t rem = count % 8; rem; --rem) {
        __m128i i = _mm_loadu_si32(source++);
        __m128 f = _mm_cvtepi32_ps(i);
        f = _mm_mul_ss(f, mul);
        _mm_store_ss(output++, f);
    }
}

#endif

namespace pfc {

	void audio_math::scale(const audio_sample * p_source,t_size p_count,audio_sample * p_output,audio_sample p_scale)
	{
#ifdef AUDIO_MATH_NEON
        neon_scale(p_source, p_count, p_output, p_scale);
#else
		noopt_scale(p_source,p_count,p_output,p_scale);
#endif
	}
	
	void audio_math::convert_to_int16(const audio_sample * p_source,t_size p_count,t_int16 * p_output,audio_sample p_scale)
	{
		audio_sample scale = (audio_sample)(p_scale * 0x8000);
#ifdef AUDIO_MATH_SSE
        convert_to_16bit_sse2(p_source, p_count, p_output, scale);
#elif defined( AUDIO_MATH_NEON )
        neon_convert_to_int16(p_source,p_count,p_output,scale);
#else
        noopt_convert_to_16bit(p_source,p_count,p_output,scale);
#endif
	}

	audio_sample audio_math::convert_to_int16_calculate_peak(const audio_sample * p_source,t_size p_count,t_int16 * p_output,audio_sample p_scale)
	{
		convert_to_int16(p_source,p_count,p_output,p_scale);
		return p_scale * calculate_peak(p_source,p_count);
	}

	void audio_math::convert_from_int16(const t_int16 * p_source,t_size p_count,audio_sample * p_output,audio_sample p_scale)
	{
		audio_sample scale = (audio_sample) ( p_scale / (double) 0x8000 );
#ifdef AUDIO_MATH_SSE
        convert_from_int16_sse2( p_source, p_count, p_output, scale );
#elif defined( AUDIO_MATH_NEON )
        neon_convert_from_int16( p_source, p_count, p_output, scale );
#else
		noopt_convert_from_int16(p_source,p_count,p_output,scale);
#endif
	}

	void audio_math::convert_to_int32(const audio_sample * p_source,t_size p_count,t_int32 * p_output,audio_sample p_scale)
	{
		audio_sample scale = (audio_sample)(p_scale * 0x80000000ul);
#ifdef AUDIO_MATH_NEON
        neon_convert_to_int32(p_source,p_count,p_output,scale);
#elif defined(AUDIO_MATH_SSE)
        convert_to_32bit_sse2(p_source, p_count, p_output, scale);
#else
        noopt_convert_to_32bit(p_source,p_count,p_output,scale);
#endif
	}

	audio_sample audio_math::convert_to_int32_calculate_peak(const audio_sample * p_source,t_size p_count,t_int32 * p_output,audio_sample p_scale)
	{
		convert_to_int32(p_source,p_count,p_output,p_scale);
		return p_scale * calculate_peak(p_source,p_count);
	}

	void audio_math::convert_from_int32(const t_int32 * p_source,t_size p_count,audio_sample * p_output,audio_sample p_scale)
	{
		audio_sample scale = (audio_sample) ( p_scale / (double) 0x80000000ul );
        // Note: speed difference here is marginal over compiler output as of Xcode 12
#ifdef AUDIO_MATH_NEON
        neon_convert_from_int32(p_source,p_count,p_output,scale);
#elif defined(AUDIO_MATH_SSE)
        sse_convert_from_int32(p_source, p_count, p_output, scale);
#else
		noopt_convert_from_int32(p_source,p_count,p_output,scale);
#endif
	}


	audio_sample audio_math::calculate_peak(const audio_sample * p_source,t_size p_count)
	{
#ifdef AUDIO_MATH_NEON
        return neon_calculate_peak( p_source, p_count );
#elif defined(AUDIO_MATH_SSE)
        return sse_calculate_peak(p_source, p_count );
#else
		return noopt_calculate_peak(p_source,p_count);
#endif
	}

	void audio_math::remove_denormals(audio_sample * p_buffer,t_size p_count) {
#if audio_sample_size == 32
		t_uint32 * ptr = reinterpret_cast<t_uint32*>(p_buffer);
		for(;p_count;p_count--)
		{
			t_uint32 t = *ptr;
			if ((t & 0x007FFFFF) && !(t & 0x7F800000)) *ptr=0;
			ptr++;
		}
#elif audio_sample_size == 64
		t_uint64 * ptr = reinterpret_cast<t_uint64*>(p_buffer);
		for(;p_count;p_count--)
		{
			t_uint64 t = *ptr;
			if ((t & 0x000FFFFFFFFFFFFF) && !(t & 0x7FF0000000000000)) *ptr=0;
			ptr++;
		}
#else
#error unsupported
#endif
	}

	void audio_math::add_offset(audio_sample * p_buffer,audio_sample p_delta,t_size p_count) {
		for(t_size n=0;n<p_count;n++) {
			p_buffer[n] += p_delta;
		}
	}


	const audio_sample audio_math::float16scale = 65536.f;
}
