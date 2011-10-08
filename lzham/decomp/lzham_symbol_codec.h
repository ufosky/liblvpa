// File: lzham_symbol_codec.h
// See Copyright Notice and license at the end of include/lzham.h
#pragma once
#include "lzham_prefix_coding.h"

namespace lzham
{
   class symbol_codec;
   class adaptive_arith_data_model;

   const uint cSymbolCodecArithMinLen = 0x01000000U;
   const uint cSymbolCodecArithMaxLen = 0xFFFFFFFFU;

   const uint cSymbolCodecArithProbBits = 11;
   const uint cSymbolCodecArithProbScale = 1 << cSymbolCodecArithProbBits;
   const uint cSymbolCodecArithProbHalfScale = 1 << (cSymbolCodecArithProbBits - 1);
   const uint cSymbolCodecArithProbMoveBits = 5;

   typedef uint64 bit_cost_t;
   const uint32 cBitCostScaleShift = 24;
   const uint32 cBitCostScale = (1U << cBitCostScaleShift);
   const bit_cost_t cBitCostMax = UINT64_MAX;

   extern uint32 gProbCost[cSymbolCodecArithProbScale];

   class raw_quasi_adaptive_huffman_data_model
   {
   public:
      raw_quasi_adaptive_huffman_data_model(bool encoding = true, uint total_syms = 0, bool fast_encoding = false, bool use_polar_codes = false);
      raw_quasi_adaptive_huffman_data_model(const raw_quasi_adaptive_huffman_data_model& other);
      ~raw_quasi_adaptive_huffman_data_model();

      raw_quasi_adaptive_huffman_data_model& operator= (const raw_quasi_adaptive_huffman_data_model& rhs);

      void clear();

      bool init(bool encoding, uint total_syms, bool fast_encoding, bool use_polar_codes);
      bool reset();

      void rescale();
      void reset_update_rate();

      inline uint get_total_syms() const { return m_total_syms; }
      inline bit_cost_t get_cost(uint sym) const { return m_code_sizes[sym] << cBitCostScaleShift; }

   public:
      lzham::vector<uint16>            m_sym_freq;

      lzham::vector<uint16>            m_codes;
      lzham::vector<uint8>             m_code_sizes;

      prefix_coding::decoder_tables*   m_pDecode_tables;

      uint                             m_total_syms;

      uint                             m_max_cycle;
      uint                             m_update_cycle;
      uint                             m_symbols_until_update;

      uint                             m_total_count;

      uint8                            m_decoder_table_bits;
      bool                             m_encoding;
      bool                             m_fast_updating;
      bool                             m_use_polar_codes;

      bool update();

      friend class symbol_codec;
   };

   struct quasi_adaptive_huffman_data_model : public raw_quasi_adaptive_huffman_data_model
   {
#if LZHAM_64BIT_POINTERS
      // Ensures sizeof(quasi_adaptive_huffman_data_model) is 128 bytes on x64 (it's 64 on x86).
      char m_unused_alignment[128 - sizeof(raw_quasi_adaptive_huffman_data_model)];
#endif
   };

   class adaptive_bit_model
   {
   public:
      adaptive_bit_model();
      adaptive_bit_model(float prob0);
      adaptive_bit_model(const adaptive_bit_model& other);

      adaptive_bit_model& operator= (const adaptive_bit_model& rhs);

      void clear();
      void set_probability_0(float prob0);
      void update(uint bit);

      inline bit_cost_t get_cost(uint bit) const { return gProbCost[bit ? (cSymbolCodecArithProbScale - m_bit_0_prob) : m_bit_0_prob]; }

   public:
      uint16 m_bit_0_prob;

      friend class symbol_codec;
      friend class adaptive_arith_data_model;
   };

   // This class is not actually used by LZHAM - it's only here for comparison/experimental purposes.
   class adaptive_arith_data_model
   {
   public:
      adaptive_arith_data_model(bool encoding = true, uint total_syms = 0);
      adaptive_arith_data_model(const adaptive_arith_data_model& other);
      ~adaptive_arith_data_model();

      adaptive_arith_data_model& operator= (const adaptive_arith_data_model& rhs);

      void clear();

      bool init(bool encoding, uint total_syms);
      bool init(bool encoding, uint total_syms, bool fast_encoding, bool use_polar_codes = false) { fast_encoding, use_polar_codes; return init(encoding, total_syms); }
      void reset();

      uint get_total_syms() const { return m_total_syms; }
      bit_cost_t get_cost(uint sym) const;

   private:
      uint m_total_syms;
      typedef lzham::vector<adaptive_bit_model> adaptive_bit_model_vector;
      adaptive_bit_model_vector m_probs;

      friend class symbol_codec;
   };

#if LZHAM_CPU_HAS_64BIT_REGISTERS
   #define LZHAM_SYMBOL_CODEC_USE_64_BIT_BUFFER 1
#else
   #define LZHAM_SYMBOL_CODEC_USE_64_BIT_BUFFER 0
#endif

   class symbol_codec
   {
   public:
      symbol_codec();

      void clear();

      // Encoding
      bool start_encoding(uint expected_file_size);
      bool encode_bits(uint bits, uint num_bits);
      bool encode_arith_init();
      bool encode_align_to_byte();
      bool encode(uint sym, quasi_adaptive_huffman_data_model& model);
      bool encode(uint bit, adaptive_bit_model& model, bool update_model = true);
      bool encode(uint sym, adaptive_arith_data_model& model);

      inline uint encode_get_total_bits_written() const { return m_total_bits_written; }

      bool stop_encoding(bool support_arith);

      const lzham::vector<uint8>& get_encoding_buf() const  { return m_output_buf; }
            lzham::vector<uint8>& get_encoding_buf()        { return m_output_buf; }

      // Decoding

      typedef void (*need_bytes_func_ptr)(size_t num_bytes_consumed, void *pPrivate_data, const uint8* &pBuf, size_t &buf_size, bool &eof_flag);

      bool start_decoding(const uint8* pBuf, size_t buf_size, bool eof_flag = true, need_bytes_func_ptr pNeed_bytes_func = NULL, void *pPrivate_data = NULL);

      inline void decode_set_input_buffer(const uint8* pBuf, size_t buf_size, const uint8* pBuf_next, bool eof_flag)
      {
         m_pDecode_buf = pBuf;
         m_pDecode_buf_next = pBuf_next;
         m_decode_buf_size = buf_size;
         m_pDecode_buf_end = pBuf + buf_size;
         m_decode_buf_eof = eof_flag;
      }
      inline uint64 decode_get_bytes_consumed() const { return m_pDecode_buf_next - m_pDecode_buf; }
      inline uint64 decode_get_bits_remaining() const { return ((m_pDecode_buf_end - m_pDecode_buf_next) << 3) + m_bit_count; }

      void start_arith_decoding();
      uint decode_bits(uint num_bits);
      uint decode_peek_bits(uint num_bits);
      void decode_remove_bits(uint num_bits);
      void decode_align_to_byte();
      int decode_remove_byte_from_bit_buf();
      uint decode(quasi_adaptive_huffman_data_model& model);
      uint decode(adaptive_bit_model& model, bool update_model = true);
      uint decode(adaptive_arith_data_model& model);
      uint64 stop_decoding();

      uint get_total_model_updates() const { return m_total_model_updates; }

   public:
      const uint8*            m_pDecode_buf;
      const uint8*            m_pDecode_buf_next;
      const uint8*            m_pDecode_buf_end;
      size_t                  m_decode_buf_size;
      bool                    m_decode_buf_eof;

      need_bytes_func_ptr     m_pDecode_need_bytes_func;
      void*                   m_pDecode_private_data;

#if LZHAM_SYMBOL_CODEC_USE_64_BIT_BUFFER
      typedef uint64 bit_buf_t;
      enum { cBitBufSize = 64 };
#else
      typedef uint32 bit_buf_t;
      enum { cBitBufSize = 32 };
#endif

      bit_buf_t               m_bit_buf;
      int                     m_bit_count;

      uint                    m_total_model_updates;

      lzham::vector<uint8>    m_output_buf;
      lzham::vector<uint8>    m_arith_output_buf;

      struct output_symbol
      {
         uint m_bits;

         enum
         {
            cArithSym = -1,
            cAlignToByteSym = -2,
            cArithInit = -3
         };
         int16 m_num_bits;

         uint16 m_arith_prob0;
      };
      lzham::vector<output_symbol> m_output_syms;

      uint                    m_total_bits_written;

      uint                    m_arith_base;
      uint                    m_arith_value;
      uint                    m_arith_length;
      uint                    m_arith_total_bits;

      quasi_adaptive_huffman_data_model*     m_pSaved_huff_model;
      adaptive_bit_model*                    m_pSaved_bit_model;

      bool put_bits_init(uint expected_size);
      bool record_put_bits(uint bits, uint num_bits);

      void arith_propagate_carry();
      bool arith_renorm_enc_interval();
      void arith_start_encoding();
      bool arith_stop_encoding();

      bool put_bits(uint bits, uint num_bits);
      bool put_bits_align_to_byte();
      bool flush_bits();
      bool assemble_output_buf();

      uint get_bits(uint num_bits);
      void remove_bits(uint num_bits);

      void decode_need_bytes();

      enum
      {
         cNull,
         cEncoding,
         cDecoding
      } m_mode;
   };

#define LZHAM_SYMBOL_CODEC_DECODE_DECLARE(codec) \
   uint arith_value = 0; \
   uint arith_length = 0; \
   symbol_codec::bit_buf_t bit_buf = 0; \
   int bit_count = 0; \
   const uint8* pDecode_buf_next = NULL;

#define LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec) \
   arith_value = codec.m_arith_value; \
   arith_length = codec.m_arith_length; \
   bit_buf = codec.m_bit_buf; \
   bit_count = codec.m_bit_count; \
   pDecode_buf_next = codec.m_pDecode_buf_next;

#define LZHAM_SYMBOL_CODEC_DECODE_END(codec) \
   codec.m_arith_value = arith_value; \
   codec.m_arith_length = arith_length; \
   codec.m_bit_buf = bit_buf; \
   codec.m_bit_count = bit_count; \
   codec.m_pDecode_buf_next = pDecode_buf_next;

// The user must declare the LZHAM_DECODE_NEEDS_BYTES macro.

#define LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, result, num_bits) \
{ \
   while (LZHAM_BUILTIN_EXPECT(bit_count < (int)(num_bits), 0)) \
   { \
      uint c; \
      if (LZHAM_BUILTIN_EXPECT(pDecode_buf_next == codec.m_pDecode_buf_end, 0)) \
      { \
         if (LZHAM_BUILTIN_EXPECT(!codec.m_decode_buf_eof, 1)) \
         { \
            LZHAM_SYMBOL_CODEC_DECODE_END(codec) \
            LZHAM_DECODE_NEEDS_BYTES \
            LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec) \
         } \
         c = 0; \
         if (LZHAM_BUILTIN_EXPECT(pDecode_buf_next < codec.m_pDecode_buf_end, 1)) c = *pDecode_buf_next++; \
      } \
      else \
         c = *pDecode_buf_next++; \
      bit_count += 8; \
      bit_buf |= (static_cast<symbol_codec::bit_buf_t>(c) << (symbol_codec::cBitBufSize - bit_count)); \
   } \
   result = num_bits ? static_cast<uint>(bit_buf >> (symbol_codec::cBitBufSize - (num_bits))) : 0; \
   bit_buf <<= (num_bits); \
   bit_count -= (num_bits); \
}

#define LZHAM_SYMBOL_CODEC_DECODE_ARITH_BIT(codec, result, model) \
{ \
   adaptive_bit_model *pModel; \
   pModel = &model; \
   while (LZHAM_BUILTIN_EXPECT(arith_length < cSymbolCodecArithMinLen, 0)) \
   { \
      uint c; codec.m_pSaved_bit_model = pModel; \
      LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, c, 8); \
      pModel = codec.m_pSaved_bit_model; \
      arith_value = (arith_value << 8) | c; \
      arith_length <<= 8; \
   } \
   uint x = pModel->m_bit_0_prob * (arith_length >> cSymbolCodecArithProbBits); \
   result = (arith_value >= x); \
   if (!result) \
   { \
      pModel->m_bit_0_prob += ((cSymbolCodecArithProbScale - pModel->m_bit_0_prob) >> cSymbolCodecArithProbMoveBits); \
      arith_length = x; \
   } \
   else \
   { \
      pModel->m_bit_0_prob -= (pModel->m_bit_0_prob >> cSymbolCodecArithProbMoveBits); \
      arith_value  -= x; \
      arith_length -= x; \
   } \
}

#if LZHAM_SYMBOL_CODEC_USE_64_BIT_BUFFER
#define LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, result, model) \
{ \
   quasi_adaptive_huffman_data_model* pModel; const prefix_coding::decoder_tables* pTables; \
   pModel = &model; pTables = model.m_pDecode_tables; \
   if (LZHAM_BUILTIN_EXPECT(bit_count < 24, 0)) \
   { \
      uint c; \
      pDecode_buf_next += sizeof(uint32); \
      if (LZHAM_BUILTIN_EXPECT(pDecode_buf_next >= codec.m_pDecode_buf_end, 0)) \
      { \
         pDecode_buf_next -= sizeof(uint32); \
         while (bit_count < 24) \
         { \
            if (!codec.m_decode_buf_eof) \
            { \
               codec.m_pSaved_huff_model = pModel; \
               LZHAM_SYMBOL_CODEC_DECODE_END(codec) \
               LZHAM_DECODE_NEEDS_BYTES \
               LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec) \
               pModel = codec.m_pSaved_huff_model; pTables = pModel->m_pDecode_tables; \
            } \
            c = 0; if (pDecode_buf_next < codec.m_pDecode_buf_end) c = *pDecode_buf_next++; \
            bit_count += 8; \
            bit_buf |= (static_cast<symbol_codec::bit_buf_t>(c) << (symbol_codec::cBitBufSize - bit_count)); \
         } \
      } \
      else \
      { \
         c = LZHAM_READ_BIG_ENDIAN_UINT32(pDecode_buf_next - sizeof(uint32)); \
         bit_count += 32; \
         bit_buf |= (static_cast<symbol_codec::bit_buf_t>(c) << (symbol_codec::cBitBufSize - bit_count)); \
      } \
   } \
   uint k = static_cast<uint>((bit_buf >> (symbol_codec::cBitBufSize - 16)) + 1); \
   uint len; \
   if (LZHAM_BUILTIN_EXPECT(k <= pTables->m_table_max_code, 1)) \
   { \
      uint32 t = pTables->m_lookup[bit_buf >> (symbol_codec::cBitBufSize - pTables->m_table_bits)]; \
      result = t & UINT16_MAX; \
      len = t >> 16; \
   } \
   else \
   { \
      len = pTables->m_decode_start_code_size; \
      for ( ; ; ) \
      { \
         if (LZHAM_BUILTIN_EXPECT(k <= pTables->m_max_codes[len - 1], 0)) \
            break; \
         len++; \
      } \
      int val_ptr = pTables->m_val_ptrs[len - 1] + static_cast<int>(bit_buf >> (symbol_codec::cBitBufSize - len)); \
      if (((uint)val_ptr >= pModel->m_total_syms)) val_ptr = 0; \
      result = pTables->m_sorted_symbol_order[val_ptr]; \
   }  \
   bit_buf <<= len; \
   bit_count -= len; \
   uint freq = pModel->m_sym_freq[result]; \
   freq++; \
   pModel->m_sym_freq[result] = static_cast<uint16>(freq); \
   LZHAM_ASSERT(freq <= UINT16_MAX); \
   if (LZHAM_BUILTIN_EXPECT(--pModel->m_symbols_until_update == 0, 0)) \
   { \
      pModel->update(); \
   } \
}
#else
#define LZHAM_SYMBOL_CODEC_DECODE_ADAPTIVE_HUFFMAN(codec, result, model) \
{ \
   quasi_adaptive_huffman_data_model* pModel; const prefix_coding::decoder_tables* pTables; \
   pModel = &model; pTables = model.m_pDecode_tables; \
   while (LZHAM_BUILTIN_EXPECT(bit_count < (symbol_codec::cBitBufSize - 8), 1)) \
   { \
      uint c; \
      if (LZHAM_BUILTIN_EXPECT(pDecode_buf_next == codec.m_pDecode_buf_end, 0)) \
      { \
         if (LZHAM_BUILTIN_EXPECT(!codec.m_decode_buf_eof, 1)) \
         { \
            codec.m_pSaved_huff_model = pModel; \
            LZHAM_SYMBOL_CODEC_DECODE_END(codec) \
            LZHAM_DECODE_NEEDS_BYTES \
            LZHAM_SYMBOL_CODEC_DECODE_BEGIN(codec) \
            pModel = codec.m_pSaved_huff_model; pTables = pModel->m_pDecode_tables; \
         } \
         c = 0; if (LZHAM_BUILTIN_EXPECT(pDecode_buf_next < codec.m_pDecode_buf_end, 1)) c = *pDecode_buf_next++; \
      } \
      else \
         c = *pDecode_buf_next++; \
      bit_count += 8; \
      bit_buf |= (static_cast<symbol_codec::bit_buf_t>(c) << (symbol_codec::cBitBufSize - bit_count)); \
   } \
   uint k = static_cast<uint>((bit_buf >> (symbol_codec::cBitBufSize - 16)) + 1); \
   uint len; \
   if (LZHAM_BUILTIN_EXPECT(k <= pTables->m_table_max_code, 1)) \
   { \
      uint32 t = pTables->m_lookup[bit_buf >> (symbol_codec::cBitBufSize - pTables->m_table_bits)]; \
      result = t & UINT16_MAX; \
      len = t >> 16; \
   } \
   else \
   { \
      len = pTables->m_decode_start_code_size; \
      for ( ; ; ) \
      { \
         if (LZHAM_BUILTIN_EXPECT(k <= pTables->m_max_codes[len - 1], 0)) \
            break; \
         len++; \
      } \
      int val_ptr = pTables->m_val_ptrs[len - 1] + static_cast<int>(bit_buf >> (symbol_codec::cBitBufSize - len)); \
      if (LZHAM_BUILTIN_EXPECT(((uint)val_ptr >= pModel->m_total_syms), 0)) val_ptr = 0; \
      result = pTables->m_sorted_symbol_order[val_ptr]; \
   }  \
   bit_buf <<= len; \
   bit_count -= len; \
   uint freq = pModel->m_sym_freq[result]; \
   freq++; \
   pModel->m_sym_freq[result] = static_cast<uint16>(freq); \
   LZHAM_ASSERT(freq <= UINT16_MAX); \
   if (LZHAM_BUILTIN_EXPECT(--pModel->m_symbols_until_update == 0, 0)) \
   { \
      pModel->update(); \
   } \
}
#endif

#define LZHAM_SYMBOL_CODEC_DECODE_ALIGN_TO_BYTE(codec) if (bit_count & 7) { int dummy_result; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, dummy_result, bit_count & 7); }

#define LZHAM_SYMBOL_CODEC_DECODE_REMOVE_BYTE_FROM_BIT_BUF(codec, result) \
{ \
   result = -1; \
   if (bit_count >= 8) \
   { \
      result = static_cast<int>(bit_buf >> (symbol_codec::cBitBufSize - 8)); \
      bit_buf <<= 8; \
      bit_count -= 8; \
   } \
}

#define LZHAM_SYMBOL_CODEC_DECODE_ARITH_START0(codec) arith_length = cSymbolCodecArithMaxLen; arith_value = 0; { uint val; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, val, 8); arith_value = val << 24; }
#define LZHAM_SYMBOL_CODEC_DECODE_ARITH_START1(codec) { uint val; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, val, 8); arith_value |= (val << 16); }
#define LZHAM_SYMBOL_CODEC_DECODE_ARITH_START2(codec) { uint val; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, val, 8); arith_value |= (val << 8); }
#define LZHAM_SYMBOL_CODEC_DECODE_ARITH_START3(codec) { uint val; LZHAM_SYMBOL_CODEC_DECODE_GET_BITS(codec, val, 8); arith_value |= val; }

} // namespace lzham

