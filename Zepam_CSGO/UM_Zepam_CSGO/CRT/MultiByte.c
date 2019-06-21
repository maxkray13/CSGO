#include "CRT.h"
#include "MultiByte.h"
#include "nls.h"
#include "utf.h"

#define NLS_CP_CPINFO 0x10000000

#define NLS_CP_MBTOWC 0x40000000

#define NLS_CP_WCTOMB 0x80000000

PTBL_PTRS        pTblPtrs;           // ptr to structure of table ptrs

UINT             gAnsiCodePage = 0xFFFFFFFF;      // Ansi code page value
UINT             gOemCodePage = 0xFFFFFFFF;       // OEM code page value
PCP_HASH         gpACPHashN;         // ptr to ACP hash node
PCP_HASH         gpOEMCPHashN;       // ptr to OEMCP hash node

////////////////////////////////////////////////////////////////////////////
//
//  GET_HASH_VALUE
//
//  Returns the hash value for given value and the given table size.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_HASH_VALUE(Value, TblSize)      (Value % TblSize)

////////////////////////////////////////////////////////////////////////////
//
//  FIND_CP_HASH_NODE
//
//  Searches for the cp hash node for the given locale.  The result is
//  put in pHashN.  If no node exists, pHashN will be NULL.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define FIND_CP_HASH_NODE( CodePage,                                       \
                           pHashN )                                        \
{                                                                          \
    UINT Index;                   /* hash value */                         \
                                                                           \
                                                                           \
    /*                                                                     \
     *  Get hash value.                                                    \
     */                                                                    \
    Index = GET_HASH_VALUE(CodePage, CP_TBL_SIZE);                         \
                                                                           \
    /*                                                                     \
     *  Make sure the hash node still doesn't exist in the table.          \
     */                                                                    \
    pHashN = (pTblPtrs->pCPHashTbl)[Index];                                \
    while ((pHashN != NULL) && (pHashN->CodePage != CodePage))             \
    {                                                                      \
        pHashN = pHashN->pNext;                                            \
    }                                                                      \
}


#define SEM_NOERROR   (SEM_FAILCRITICALERRORS |     \
                       SEM_NOGPFAULTERRORBOX  |     \
                       SEM_NOOPENFILEERRORBOX)




//-------------------------------------------------------------------------//
//                           INTERNAL MACROS                               //
//-------------------------------------------------------------------------//


////////////////////////////////////////////////////////////////////////////
//
//  CHECK_DBCS_LEAD_BYTE
//
//  Returns the offset to the DBCS table for the given leadbyte character.
//  If the given character is not a leadbyte, then it returns zero (table
//  value).
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define CHECK_DBCS_LEAD_BYTE(pDBCSOff, Ch)                                 \
    (pDBCSOff ? ((WORD)(pDBCSOff[Ch])) : ((WORD)0))


////////////////////////////////////////////////////////////////////////////
//
//  CHECK_ERROR_WC_SINGLE
//
//  Checks to see if the default character was used due to an invalid
//  character.  Sets last error and returns 0 characters written if an
//  invalid character was used.
//
//  NOTE: This macro may return if an error is encountered.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define CHECK_ERROR_WC_SINGLE( pHashN,                                     \
                               wch,                                        \
                               Ch )                                        \
{                                                                          \
    if ( ( (wch == pHashN->pCPInfo->wUniDefaultChar) &&                    \
           (Ch != pHashN->pCPInfo->wTransUniDefaultChar) ) ||              \
         ( (wch >= PRIVATE_USE_BEGIN) && (wch <= PRIVATE_USE_END) ) )      \
    {                                                                      \
        (ERROR_NO_UNICODE_TRANSLATION);                        \
        return (0);                                                        \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  CHECK_ERROR_WC_MULTI
//
//  Checks to see if the default character was used due to an invalid
//  character.  Sets last error and returns 0 characters written if an
//  invalid character was used.
//
//  NOTE: This macro may return if an error is encountered.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define CHECK_ERROR_WC_MULTI( pHashN,                                      \
                              wch,                                         \
                              lead,                                        \
                              trail )                                      \
{                                                                          \
    if ((wch == pHashN->pCPInfo->wUniDefaultChar) &&                       \
        (MAKEWORD(trail, lead) != pHashN->pCPInfo->wTransUniDefaultChar))  \
    {                                                                      \
        (ERROR_NO_UNICODE_TRANSLATION);                        \
        return (0);                                                        \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  CHECK_ERROR_WC_MULTI_SPECIAL
//
//  Checks to see if the default character was used due to an invalid
//  character.  Sets it to 0xffff if invalid.
//
//  DEFINED AS A MACRO.
//
//  08-21-95    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define CHECK_ERROR_WC_MULTI_SPECIAL( pHashN,                              \
                                      pWCStr,                              \
                                      lead,                                \
                                      trail )                              \
{                                                                          \
    if ((*pWCStr == pHashN->pCPInfo->wUniDefaultChar) &&                   \
        (MAKEWORD(trail, lead) != pHashN->pCPInfo->wTransUniDefaultChar))  \
    {                                                                      \
        *pWCStr = 0xffff;                                                  \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_WC_SINGLE
//
//  Fills in pWCStr with the wide character(s) for the corresponding single
//  byte character from the appropriate translation table.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_WC_SINGLE( pMBTbl,                                             \
                       pMBStr,                                             \
                       pWCStr )                                            \
{                                                                          \
    *pWCStr = pMBTbl[*pMBStr];                                             \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_WC_SINGLE_SPECIAL
//
//  Fills in pWCStr with the wide character(s) for the corresponding single
//  byte character from the appropriate translation table.  Also checks for
//  invalid characters - if invalid, it fills in 0xffff instead.
//
//  DEFINED AS A MACRO.
//
//  08-21-95    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_WC_SINGLE_SPECIAL( pHashN,                                     \
                               pMBTbl,                                     \
                               pMBStr,                                     \
                               pWCStr )                                    \
{                                                                          \
    *pWCStr = pMBTbl[*pMBStr];                                             \
                                                                           \
    if ( ( (*pWCStr == pHashN->pCPInfo->wUniDefaultChar) &&                \
           (*pMBStr != pHashN->pCPInfo->wTransUniDefaultChar) ) ||         \
         ( (*pWCStr >= PRIVATE_USE_BEGIN) &&                               \
           (*pWCStr <= PRIVATE_USE_END) ) )                                \
    {                                                                      \
        *pWCStr = 0xffff;                                                  \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_WC_MULTI
//
//  Fills in pWCStr with the wide character(s) for the corresponding multibyte
//  character from the appropriate translation table.  The number of bytes
//  used from the pMBStr buffer (single byte or double byte) is stored in
//  the mbIncr parameter.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_WC_MULTI( pHashN,                                              \
                      pMBTbl,                                              \
                      pMBStr,                                              \
                      pEndMBStr,                                           \
                      pWCStr,                                              \
                      pEndWCStr,                                           \
                      mbIncr )                                             \
{                                                                          \
    WORD Offset;                  /* offset to DBCS table for range */     \
                                                                           \
                                                                           \
    if (Offset = CHECK_DBCS_LEAD_BYTE(pHashN->pDBCSOffsets, *pMBStr))      \
    {                                                                      \
        /*                                                                 \
         *  DBCS Lead Byte.  Make sure there is a trail byte with the      \
         *  lead byte.                                                     \
         */                                                                \
        if (pMBStr + 1 == pEndMBStr)                                       \
        {                                                                  \
            /*                                                             \
             *  There is no trail byte with the lead byte.  The lead byte  \
             *  is the LAST character in the string.  Translate to NULL.   \
             */                                                            \
            *pWCStr = (WCHAR)0;                                            \
            mbIncr = 1;                                                    \
        }                                                                  \
        else if (*(pMBStr + 1) == 0)                                       \
        {                                                                  \
            /*                                                             \
             *  There is no trail byte with the lead byte.  The lead byte  \
             *  is followed by a NULL.  Translate to NULL.                 \
             *                                                             \
             *  Increment by 2 so that the null is not counted twice.      \
             */                                                            \
            *pWCStr = (WCHAR)0;                                            \
            mbIncr = 2;                                                    \
        }                                                                  \
        else                                                               \
        {                                                                  \
            /*                                                             \
             *  Fill in the wide character translation from the double     \
             *  byte character table.                                      \
             */                                                            \
            *pWCStr = (pHashN->pDBCSOffsets + Offset)[*(pMBStr + 1)];      \
            mbIncr = 2;                                                    \
        }                                                                  \
    }                                                                      \
    else                                                                   \
    {                                                                      \
        /*                                                                 \
         *  Not DBCS Lead Byte.  Fill in the wide character translation    \
         *  from the single byte character table.                          \
         */                                                                \
        *pWCStr = pMBTbl[*pMBStr];                                         \
        mbIncr = 1;                                                        \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_WC_MULTI_ERR
//
//  Fills in pWCStr with the wide character(s) for the corresponding multibyte
//  character from the appropriate translation table.  The number of bytes
//  used from the pMBStr buffer (single byte or double byte) is stored in
//  the mbIncr parameter.
//
//  Once the character has been translated, it checks to be sure the
//  character was valid.  If not, it sets last error and return 0 characters
//  written.
//
//  NOTE: This macro may return if an error is encountered.
//
//  DEFINED AS A MACRO.
//
//  09-01-93    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_WC_MULTI_ERR( pHashN,                                          \
                          pMBTbl,                                          \
                          pMBStr,                                          \
                          pEndMBStr,                                       \
                          pWCStr,                                          \
                          pEndWCStr,                                       \
                          mbIncr )                                         \
{                                                                          \
    WORD Offset;                  /* offset to DBCS table for range */     \
                                                                           \
                                                                           \
    if (Offset = CHECK_DBCS_LEAD_BYTE(pHashN->pDBCSOffsets, *pMBStr))      \
    {                                                                      \
        /*                                                                 \
         *  DBCS Lead Byte.  Make sure there is a trail byte with the      \
         *  lead byte.                                                     \
         */                                                                \
        if ((pMBStr + 1 == pEndMBStr) || (*(pMBStr + 1) == 0))             \
        {                                                                  \
            /*                                                             \
             *  There is no trail byte with the lead byte.  Return error.  \
             */                                                            \
            (ERROR_NO_UNICODE_TRANSLATION);                    \
            return (0);                                                    \
        }                                                                  \
                                                                           \
        /*                                                                 \
         *  Fill in the wide character translation from the double         \
         *  byte character table.                                          \
         */                                                                \
        *pWCStr = (pHashN->pDBCSOffsets + Offset)[*(pMBStr + 1)];          \
        mbIncr = 2;                                                        \
                                                                           \
        /*                                                                 \
         *  Make sure an invalid character was not translated to           \
         *  the default char.  Return an error if invalid.                 \
         */                                                                \
        CHECK_ERROR_WC_MULTI( pHashN,                                      \
                              *pWCStr,                                     \
                              *pMBStr,                                     \
                              *(pMBStr + 1) );                             \
    }                                                                      \
    else                                                                   \
    {                                                                      \
        /*                                                                 \
         *  Not DBCS Lead Byte.  Fill in the wide character translation    \
         *  from the single byte character table.                          \
         */                                                                \
        *pWCStr = pMBTbl[*pMBStr];                                         \
        mbIncr = 1;                                                        \
                                                                           \
        /*                                                                 \
         *  Make sure an invalid character was not translated to           \
         *  the default char.  Return an error if invalid.                 \
         */                                                                \
        CHECK_ERROR_WC_SINGLE( pHashN,                                     \
                               *pWCStr,                                    \
                               *pMBStr );                                  \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_WC_MULTI_ERR_SPECIAL
//
//  Fills in pWCStr with the wide character(s) for the corresponding multibyte
//  character from the appropriate translation table.  The number of bytes
//  used from the pMBStr buffer (single byte or double byte) is stored in
//  the mbIncr parameter.
//
//  Once the character has been translated, it checks to be sure the
//  character was valid.  If not, it fills in 0xffff.
//
//  DEFINED AS A MACRO.
//
//  08-21-95    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_WC_MULTI_ERR_SPECIAL( pHashN,                                  \
                                  pMBTbl,                                  \
                                  pMBStr,                                  \
                                  pEndMBStr,                               \
                                  pWCStr,                                  \
                                  pEndWCStr,                               \
                                  mbIncr )                                 \
{                                                                          \
    WORD Offset;                  /* offset to DBCS table for range */     \
                                                                           \
                                                                           \
    if (Offset = CHECK_DBCS_LEAD_BYTE(pHashN->pDBCSOffsets, *pMBStr))      \
    {                                                                      \
        /*                                                                 \
         *  DBCS Lead Byte.  Make sure there is a trail byte with the      \
         *  lead byte.                                                     \
         */                                                                \
        if ((pMBStr + 1 == pEndMBStr) || (*(pMBStr + 1) == 0))             \
        {                                                                  \
            /*                                                             \
             *  There is no trail byte with the lead byte.  The lead byte  \
             *  is the LAST character in the string.  Translate to 0xffff. \
             */                                                            \
            *pWCStr = (WCHAR)0xffff;                                       \
            mbIncr = 1;                                                    \
        }                                                                  \
        else                                                               \
        {                                                                  \
            /*                                                             \
             *  Fill in the wide character translation from the double     \
             *  byte character table.                                      \
             */                                                            \
            *pWCStr = (pHashN->pDBCSOffsets + Offset)[*(pMBStr + 1)];      \
            mbIncr = 2;                                                    \
                                                                           \
            /*                                                             \
             *  Make sure an invalid character was not translated to       \
             *  the default char.  Translate to 0xffff if invalid.         \
             */                                                            \
            CHECK_ERROR_WC_MULTI_SPECIAL( pHashN,                          \
                                          pWCStr,                          \
                                          *pMBStr,                         \
                                          *(pMBStr + 1) );                 \
        }                                                                  \
    }                                                                      \
    else                                                                   \
    {                                                                      \
        /*                                                                 \
         *  Not DBCS Lead Byte.  Fill in the wide character translation    \
         *  from the single byte character table.                          \
         *  Make sure an invalid character was not translated to           \
         *  the default char.  Return an error if invalid.                 \
         */                                                                \
        GET_WC_SINGLE_SPECIAL( pHashN,                                     \
                               pMBTbl,                                     \
                               pMBStr,                                     \
                               pWCStr );                                   \
        mbIncr = 1;                                                        \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  COPY_MB_CHAR
//
//  Copies a multibyte character to the given string buffer.  If the
//  high byte of the multibyte word is zero, then it is a single byte
//  character and the number of characters written (returned) is 1.
//  Otherwise, it is a double byte character and the number of characters
//  written (returned) is 2.
//
//  NumByte will be 0 if the buffer is too small for the translation.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define COPY_MB_CHAR( mbChar,                                              \
                      pMBStr,                                              \
                      NumByte,                                             \
                      fOnlyOne )                                           \
{                                                                          \
    if (HIBYTE(mbChar))                                                    \
    {                                                                      \
        /*                                                                 \
         *  Make sure there is enough room in the buffer for both bytes.   \
         */                                                                \
        if (fOnlyOne)                                                      \
        {                                                                  \
            NumByte = 0;                                                   \
        }                                                                  \
        else                                                               \
        {                                                                  \
            /*                                                             \
             *  High Byte is NOT zero, so it's a DOUBLE byte char.         \
             *  Return 2 characters written.                               \
             */                                                            \
            *pMBStr = HIBYTE(mbChar);                                      \
            *(pMBStr + 1) = LOBYTE(mbChar);                                \
            NumByte = 2;                                                   \
        }                                                                  \
    }                                                                      \
    else                                                                   \
    {                                                                      \
        /*                                                                 \
         *  High Byte IS zero, so it's a SINGLE byte char.                 \
         *  Return 1 character written.                                    \
         */                                                                \
        *pMBStr = LOBYTE(mbChar);                                          \
        NumByte = 1;                                                       \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_SB
//
//  Fills in pMBStr with the single byte character for the corresponding
//  wide character from the appropriate translation table.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_SB( pWC,                                                       \
                wChar,                                                     \
                pMBStr )                                                   \
{                                                                          \
    *pMBStr = ((BYTE *)(pWC))[wChar];                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_MB
//
//  Fills in pMBStr with the multi byte character for the corresponding
//  wide character from the appropriate translation table.
//
//  mbCount will be 0 if the buffer is too small for the translation.
//
//    Broken Down Version:
//    --------------------
//        mbChar = ((WORD *)(pHashN->pWC))[wChar];
//        COPY_MB_CHAR(mbChar, pMBStr, mbCount);
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_MB( pWC,                                                       \
                wChar,                                                     \
                pMBStr,                                                    \
                mbCount,                                                   \
                fOnlyOne )                                                 \
{                                                                          \
    COPY_MB_CHAR( ((WORD *)(pWC))[wChar],                                  \
                  pMBStr,                                                  \
                  mbCount,                                                 \
                  fOnlyOne );                                              \
}


////////////////////////////////////////////////////////////////////////////
//
//  ELIMINATE_BEST_FIT_SB
//
//  Checks to see if a single byte Best Fit character was used.  If so,
//  it replaces it with a single byte default character.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define ELIMINATE_BEST_FIT_SB( pHashN,                                     \
                               wChar,                                      \
                               pMBStr )                                    \
{                                                                          \
    if ((pHashN->pMBTbl)[*pMBStr] != wChar)                                \
    {                                                                      \
        *pMBStr = LOBYTE(pHashN->pCPInfo->wDefaultChar);                   \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  ELIMINATE_BEST_FIT_MB
//
//  Checks to see if a multi byte Best Fit character was used.  If so,
//  it replaces it with a multi byte default character.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define ELIMINATE_BEST_FIT_MB( pHashN,                                     \
                               wChar,                                      \
                               pMBStr,                                     \
                               mbCount,                                    \
                               fOnlyOne )                                  \
{                                                                          \
    WORD Offset;                                                           \
    WORD wDefault_v;                                                         \
                                                                           \
    if (((mbCount == 1) && ((pHashN->pMBTbl)[*pMBStr] != wChar)) ||        \
        ((mbCount == 2) &&                                                 \
         (Offset = CHECK_DBCS_LEAD_BYTE(pHashN->pDBCSOffsets, *pMBStr)) && \
         (((pHashN->pDBCSOffsets + Offset)[*(pMBStr + 1)]) != wChar)))     \
    {                                                                      \
        wDefault_v = pHashN->pCPInfo->wDefaultChar;                          \
        if (HIBYTE(wDefault_v))                                              \
        {                                                                  \
            if (fOnlyOne)                                                  \
            {                                                              \
                mbCount = 0;                                               \
            }                                                              \
            else                                                           \
            {                                                              \
                *pMBStr = HIBYTE(wDefault_v);                                \
                *(pMBStr + 1) = LOBYTE(wDefault_v);                          \
                mbCount = 2;                                               \
            }                                                              \
        }                                                                  \
        else                                                               \
        {                                                                  \
            *pMBStr = LOBYTE(wDefault_v);                                    \
            mbCount = 1;                                                   \
        }                                                                  \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_DEFAULT_WORD
//
//  Takes a pointer to a character string (either one or two characters),
//  and converts it to a WORD value.  If the character is not DBCS, then it
//  zero extends the high byte.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_DEFAULT_WORD(pOff, pDefault)                                   \
    (CHECK_DBCS_LEAD_BYTE(pOff, *pDefault)                                 \
         ? MAKEWORD(*(pDefault + 1), *pDefault)                            \
         : MAKEWORD(*pDefault, 0))


////////////////////////////////////////////////////////////////////////////
//
//  DEFAULT_CHAR_CHECK_SB
//
//  Checks to see if the default character is used.  If it is, it sets
//  pUsedDef to TRUE (if non-null).  If the user specified a default, then
//  the user's default character is used.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define DEFAULT_CHAR_CHECK_SB( pHashN,                                     \
                               wch,                                        \
                               pMBStr,                                     \
                               wDefChar,                                   \
                               pUsedDef )                                  \
{                                                                          \
    WORD wSysDefChar = pHashN->pCPInfo->wDefaultChar;                      \
                                                                           \
                                                                           \
    /*                                                                     \
     *  Check for default character being used.                            \
     */                                                                    \
    if ((*pMBStr == (BYTE)wSysDefChar) &&                                  \
        (wch != pHashN->pCPInfo->wTransDefaultChar))                       \
    {                                                                      \
        /*                                                                 \
         *  Default was used.  Set the pUsedDef parameter to TRUE.         \
         */                                                                \
        *pUsedDef = TRUE;                                                  \
                                                                           \
        /*                                                                 \
         *  If the user specified a different default character than       \
         *  the system default, use that character instead.                \
         */                                                                \
        if (wSysDefChar != wDefChar)                                       \
        {                                                                  \
            *pMBStr = LOBYTE(wDefChar);                                    \
        }                                                                  \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  DEFAULT_CHAR_CHECK_MB
//
//  Checks to see if the default character is used.  If it is, it sets
//  pUsedDef to TRUE (if non-null).  If the user specified a default, then
//  the user's default character is used.  The number of bytes written to
//  the buffer is returned.
//
//  NumByte will be -1 if the buffer is too small for the translation.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define DEFAULT_CHAR_CHECK_MB( pHashN,                                     \
                               wch,                                        \
                               pMBStr,                                     \
                               wDefChar,                                   \
                               pUsedDef,                                   \
                               NumByte,                                    \
                               fOnlyOne )                                  \
{                                                                          \
    WORD wSysDefChar = pHashN->pCPInfo->wDefaultChar;                      \
                                                                           \
                                                                           \
    /*                                                                     \
     *  Set NumByte to zero for return (zero bytes written).               \
     */                                                                    \
    NumByte = 0;                                                           \
                                                                           \
    /*                                                                     \
     *  Check for default character being used.                            \
     */                                                                    \
    if ((*pMBStr == (BYTE)wSysDefChar) &&                                  \
        (wch != pHashN->pCPInfo->wTransDefaultChar))                       \
    {                                                                      \
        /*                                                                 \
         *  Default was used.  Set the pUsedDef parameter to TRUE.         \
         */                                                                \
        *pUsedDef = TRUE;                                                  \
                                                                           \
        /*                                                                 \
         *  If the user specified a different default character than       \
         *  the system default, use that character instead.                \
         */                                                                \
        if (wSysDefChar != wDefChar)                                       \
        {                                                                  \
            COPY_MB_CHAR( wDefChar,                                        \
                          pMBStr,                                          \
                          NumByte,                                         \
                          fOnlyOne );                                      \
            if (NumByte == 0)                                              \
            {                                                              \
                NumByte = -1;                                              \
            }                                                              \
        }                                                                  \
    }                                                                      \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_WC_TRANSLATION_SB
//
//  Gets the 1:1 translation of a given wide character.  It fills in the
//  string pointer with the single byte character.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_WC_TRANSLATION_SB( pHashN,                                     \
                               wch,                                        \
                               pMBStr,                                     \
                               wDefault,                                   \
                               pUsedDef,                                   \
                               dwFlags )                                   \
{                                                                          \
    GET_SB( pHashN->pWC,                                                   \
            wch,                                                           \
            pMBStr );                                                      \
    if (dwFlags & WC_NO_BEST_FIT_CHARS)                                    \
    {                                                                      \
        ELIMINATE_BEST_FIT_SB( pHashN,                                     \
                               wch,                                        \
                               pMBStr );                                   \
    }                                                                      \
    DEFAULT_CHAR_CHECK_SB( pHashN,                                         \
                           wch,                                            \
                           pMBStr,                                         \
                           wDefault,                                       \
                           pUsedDef );                                     \
}


////////////////////////////////////////////////////////////////////////////
//
//  GET_WC_TRANSLATION_MB
//
//  Gets the 1:1 translation of a given wide character.  It fills in the
//  appropriate number of characters for the multibyte character and then
//  returns the number of characters written to the multibyte string.
//
//  mbCnt will be 0 if the buffer is too small for the translation.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_WC_TRANSLATION_MB( pHashN,                                     \
                               wch,                                        \
                               pMBStr,                                     \
                               wDefault,                                   \
                               pUsedDef,                                   \
                               mbCnt,                                      \
                               fOnlyOne,                                   \
                               dwFlags )                                   \
{                                                                          \
    int mbCnt2;              /* number of characters written */            \
                                                                           \
                                                                           \
    GET_MB( pHashN->pWC,                                                   \
            wch,                                                           \
            pMBStr,                                                        \
            mbCnt,                                                         \
            fOnlyOne );                                                    \
    if (dwFlags & WC_NO_BEST_FIT_CHARS)                                    \
    {                                                                      \
        ELIMINATE_BEST_FIT_MB( pHashN,                                     \
                               wch,                                        \
                               pMBStr,                                     \
                               mbCnt,                                      \
                               fOnlyOne );                                 \
    }                                                                      \
    if (mbCnt)                                                             \
    {                                                                      \
        DEFAULT_CHAR_CHECK_MB( pHashN,                                     \
                               wch,                                        \
                               pMBStr,                                     \
                               wDefault,                                   \
                               pUsedDef,                                   \
                               mbCnt2,                                     \
                               fOnlyOne );                                 \
        if (mbCnt2 == -1)                                                  \
        {                                                                  \
            mbCnt = 0;                                                     \
        }                                                                  \
        else if (mbCnt2)                                                   \
        {                                                                  \
            mbCnt = mbCnt2;                                                \
        }                                                                  \
    }                                                                      \
}



////////////////////////////////////////////////////////////////////////////
//
//  GetMBNoDefault
//
//  Translates the wide character string to a multibyte string and returns
//  the number of bytes written.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetMBNoDefault(
	PCP_HASH pHashN,
	LPWSTR pWCStr,
	LPWSTR pEndWCStr,
	LPBYTE pMBStr,
	int cbMultiByte,
	DWORD dwFlags)
{
	int mbIncr;                   // amount to increment pMBStr
	int mbCount = 0;              // count of multibyte chars written
	LPBYTE pEndMBStr;             // ptr to end of MB string buffer
	PWC_TABLE pWC = pHashN->pWC;  // ptr to WC table
	int ctr;                      // loop counter


	//
	//  If cbMultiByte is 0, then we can't use pMBStr.  In this
	//  case, we simply want to count the number of characters that
	//  would be written to the buffer.
	//
	if (cbMultiByte == 0)
	{
		BYTE pTempStr[2];             // tmp buffer - 2 bytes for DBCS

		//
		//  For each wide char, translate it to its corresponding multibyte
		//  char and increment the multibyte character count.
		//
		if (IS_SBCS_CP(pHashN))
		{
			//
			//  Single Byte Character Code Page.
			//
			//  Just return the count of characters - it will be the
			//  same number of characters as the source string.
			//
			mbCount = (int)(pEndWCStr - pWCStr);
		}
		else
		{
			//
			//  Multi Byte Character Code Page.
			//
			if (dwFlags & WC_NO_BEST_FIT_CHARS)
			{
				while (pWCStr < pEndWCStr)
				{
					GET_MB(pWC,
						*pWCStr,
						pTempStr,
						mbIncr,
						FALSE);
					ELIMINATE_BEST_FIT_MB(pHashN,
						*pWCStr,
						pTempStr,
						mbIncr,
						FALSE);
					pWCStr++;
					mbCount += mbIncr;
				}
			}
			else
			{
				while (pWCStr < pEndWCStr)
				{
					GET_MB(pWC,
						*pWCStr,
						pTempStr,
						mbIncr,
						FALSE);
					pWCStr++;
					mbCount += mbIncr;
				}
			}
		}
	}
	else
	{
		//
		//  Initialize multibyte loop pointers.
		//
		pEndMBStr = pMBStr + cbMultiByte;

		//
		//  For each wide char, translate it to its corresponding
		//  multibyte char, store it in pMBStr, and increment the
		//  multibyte character count.
		//
		if (IS_SBCS_CP(pHashN))
		{
			//
			//  Single Byte Character Code Page.
			//
			mbCount = (int)(pEndWCStr - pWCStr);
			if ((pEndMBStr - pMBStr) < mbCount)
			{
				mbCount = (int)(pEndMBStr - pMBStr);
			}
			if (dwFlags & WC_NO_BEST_FIT_CHARS)
			{
				for (ctr = mbCount; ctr > 0; ctr--)
				{
					GET_SB(pWC,
						*pWCStr,
						pMBStr);
					ELIMINATE_BEST_FIT_SB(pHashN,
						*pWCStr,
						pMBStr);
					pWCStr++;
					pMBStr++;
				}
			}
			else
			{
				for (ctr = mbCount; ctr > 0; ctr--)
				{
					GET_SB(pWC,
						*pWCStr,
						pMBStr);
					pWCStr++;
					pMBStr++;
				}
			}
		}
		else
		{
			//
			//  Multi Byte Character Code Page.
			//
			if (dwFlags & WC_NO_BEST_FIT_CHARS)
			{
				while ((pWCStr < pEndWCStr) && (pMBStr < pEndMBStr))
				{
					GET_MB(pWC,
						*pWCStr,
						pMBStr,
						mbIncr,
						((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
					ELIMINATE_BEST_FIT_MB(pHashN,
						*pWCStr,
						pMBStr,
						mbIncr,
						((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
					if (mbIncr == 0)
					{
						//
						//  Not enough space in buffer.
						//
						break;
					}

					pWCStr++;
					mbCount += mbIncr;
					pMBStr += mbIncr;
				}
			}
			else
			{
				while ((pWCStr < pEndWCStr) && (pMBStr < pEndMBStr))
				{
					GET_MB(pWC,
						*pWCStr,
						pMBStr,
						mbIncr,
						((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
					if (mbIncr == 0)
					{
						//
						//  Not enough space in buffer.
						//
						break;
					}

					pWCStr++;
					mbCount += mbIncr;
					pMBStr += mbIncr;
				}
			}
		}

		//
		//  Make sure multibyte character buffer was large enough.
		//
		if (pWCStr < pEndWCStr)
		{
			(ERROR_INSUFFICIENT_BUFFER);
			return (0);
		}
	}

	//
	//  Return the number of characters written (or that would have
	//  been written) to the buffer.
	//
	return (mbCount);
}

////////////////////////////////////////////////////////////////////////////
//
//  GetMBDefault
//
//  Translates the wide character string to a multibyte string and returns
//  the number of bytes written.  This also checks for the use of the default
//  character, so the translation is slower.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetMBDefault(
	PCP_HASH pHashN,
	LPWSTR pWCStr,
	LPWSTR pEndWCStr,
	LPBYTE pMBStr,
	int cbMultiByte,
	WORD wDefault,
	LPBOOL pUsedDef,
	DWORD dwFlags)
{
	int mbIncr;                   // amount to increment pMBStr
	int mbIncr2;                  // amount to increment pMBStr
	int mbCount = 0;              // count of multibyte chars written
	LPBYTE pEndMBStr;             // ptr to end of MB string buffer
	PWC_TABLE pWC = pHashN->pWC;  // ptr to WC table
	int ctr;                      // loop counter


	//
	//  If cbMultiByte is 0, then we can't use pMBStr.  In this
	//  case, we simply want to count the number of characters that
	//  would be written to the buffer.
	//
	if (cbMultiByte == 0)
	{
		BYTE pTempStr[2];             // tmp buffer - 2 bytes for DBCS

		//
		//  For each wide char, translate it to its corresponding multibyte
		//  char and increment the multibyte character count.
		//
		if (IS_SBCS_CP(pHashN))
		{
			//
			//  Single Byte Character Code Page.
			//
			mbCount = (int)(pEndWCStr - pWCStr);
			if (dwFlags & WC_NO_BEST_FIT_CHARS)
			{
				while (pWCStr < pEndWCStr)
				{
					GET_SB(pWC,
						*pWCStr,
						pTempStr);
					ELIMINATE_BEST_FIT_SB(pHashN,
						*pWCStr,
						pTempStr);
					DEFAULT_CHAR_CHECK_SB(pHashN,
						*pWCStr,
						pTempStr,
						wDefault,
						pUsedDef);
					pWCStr++;
				}
			}
			else
			{
				while (pWCStr < pEndWCStr)
				{
					GET_SB(pWC,
						*pWCStr,
						pTempStr);
					DEFAULT_CHAR_CHECK_SB(pHashN,
						*pWCStr,
						pTempStr,
						wDefault,
						pUsedDef);
					pWCStr++;
				}
			}
		}
		else
		{
			//
			//  Multi Byte Character Code Page.
			//
			if (dwFlags & WC_NO_BEST_FIT_CHARS)
			{
				while (pWCStr < pEndWCStr)
				{
					GET_MB(pWC,
						*pWCStr,
						pTempStr,
						mbIncr,
						FALSE);
					ELIMINATE_BEST_FIT_MB(pHashN,
						*pWCStr,
						pTempStr,
						mbIncr,
						FALSE);
					DEFAULT_CHAR_CHECK_MB(pHashN,
						*pWCStr,
						pTempStr,
						wDefault,
						pUsedDef,
						mbIncr2,
						FALSE);
					mbCount += (mbIncr2) ? (mbIncr2) : (mbIncr);
					pWCStr++;
				}
			}
			else
			{
				while (pWCStr < pEndWCStr)
				{
					GET_MB(pWC,
						*pWCStr,
						pTempStr,
						mbIncr,
						FALSE);
					DEFAULT_CHAR_CHECK_MB(pHashN,
						*pWCStr,
						pTempStr,
						wDefault,
						pUsedDef,
						mbIncr2,
						FALSE);
					mbCount += (mbIncr2) ? (mbIncr2) : (mbIncr);
					pWCStr++;
				}
			}
		}
	}
	else
	{
		//
		//  Initialize multibyte loop pointers.
		//
		pEndMBStr = pMBStr + cbMultiByte;

		//
		//  For each wide char, translate it to its corresponding
		//  multibyte char, store it in pMBStr, and increment the
		//  multibyte character count.
		//
		if (IS_SBCS_CP(pHashN))
		{
			//
			//  Single Byte Character Code Page.
			//
			mbCount = (int)(pEndWCStr - pWCStr);
			if ((pEndMBStr - pMBStr) < mbCount)
			{
				mbCount = (int)(pEndMBStr - pMBStr);
			}
			if (dwFlags & WC_NO_BEST_FIT_CHARS)
			{
				for (ctr = mbCount; ctr > 0; ctr--)
				{
					GET_SB(pWC,
						*pWCStr,
						pMBStr);
					ELIMINATE_BEST_FIT_SB(pHashN,
						*pWCStr,
						pMBStr);
					DEFAULT_CHAR_CHECK_SB(pHashN,
						*pWCStr,
						pMBStr,
						wDefault,
						pUsedDef);
					pWCStr++;
					pMBStr++;
				}
			}
			else
			{
				for (ctr = mbCount; ctr > 0; ctr--)
				{
					GET_SB(pWC,
						*pWCStr,
						pMBStr);
					DEFAULT_CHAR_CHECK_SB(pHashN,
						*pWCStr,
						pMBStr,
						wDefault,
						pUsedDef);
					pWCStr++;
					pMBStr++;
				}
			}
		}
		else
		{
			//
			//  Multi Byte Character Code Page.
			//
			if (dwFlags & WC_NO_BEST_FIT_CHARS)
			{
				while ((pWCStr < pEndWCStr) && (pMBStr < pEndMBStr))
				{
					GET_MB(pWC,
						*pWCStr,
						pMBStr,
						mbIncr,
						((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
					ELIMINATE_BEST_FIT_MB(pHashN,
						*pWCStr,
						pMBStr,
						mbIncr,
						((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
					DEFAULT_CHAR_CHECK_MB(pHashN,
						*pWCStr,
						pMBStr,
						wDefault,
						pUsedDef,
						mbIncr2,
						((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
					if ((mbIncr == 0) || (mbIncr2 == -1))
					{
						//
						//  Not enough room in buffer.
						//
						break;
					}

					mbCount += (mbIncr2) ? (mbIncr2) : (mbIncr);
					pWCStr++;
					pMBStr += mbIncr;
				}
			}
			else
			{
				while ((pWCStr < pEndWCStr) && (pMBStr < pEndMBStr))
				{
					GET_MB(pWC,
						*pWCStr,
						pMBStr,
						mbIncr,
						((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
					DEFAULT_CHAR_CHECK_MB(pHashN,
						*pWCStr,
						pMBStr,
						wDefault,
						pUsedDef,
						mbIncr2,
						((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
					if ((mbIncr == 0) || (mbIncr2 == -1))
					{
						//
						//  Not enough room in buffer.
						//
						break;
					}

					mbCount += (mbIncr2) ? (mbIncr2) : (mbIncr);
					pWCStr++;
					pMBStr += mbIncr;
				}
			}
		}

		//
		//  Make sure multibyte character buffer was large enough.
		//
		if (pWCStr < pEndWCStr)
		{
			(ERROR_INSUFFICIENT_BUFFER);
			return (0);
		}
	}

	//
	//  Return the number of characters written (or that would have
	//  been written) to the buffer.
	//
	return (mbCount);
}

////////////////////////////////////////////////////////////////////////////
//
//  GetPreComposedChar
//
//  Gets the precomposed character form of a given base character and
//  nonspacing character.  If there is no precomposed form for the given
//  character, it returns 0.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

WCHAR  GetPreComposedChar(
	WCHAR wcNonSp,
	WCHAR wcBase)
{
	PCOMP_INFO pComp;             // ptr to composite information
	WORD BSOff = 0;               // offset of base char in grid
	WORD NSOff = 0;               // offset of nonspace char in grid
	int Index;                    // index into grid


	//
	//  Store the ptr to the composite information.  No need to check if
	//  it's a NULL pointer since all tables in the Unicode file are
	//  constructed during initialization.
	//
	pComp = pTblPtrs->pComposite;

	//
	//  Traverse 8:4:4 table for Base character offset.
	//
	BSOff = TRAVERSE_844_W(pComp->pBase, wcBase);
	if (!BSOff)
	{
		return (0);
	}

	//
	//  Traverse 8:4:4 table for NonSpace character offset.
	//
	NSOff = TRAVERSE_844_W(pComp->pNonSp, wcNonSp);
	if (!NSOff)
	{
		return (0);
	}

	//
	//  Get wide character value out of 2D grid.
	//  If there is no precomposed character at the location in the
	//  grid, it will return 0.
	//
	Index = (BSOff - 1) * pComp->NumNonSp + (NSOff - 1);
	return ((pComp->pGrid)[Index]);
}
////////////////////////////////////////////////////////////////////////////
//
//  GetMBCompSB
//
//  Fills in pMBStr with the byte character(s) for the corresponding wide
//  character from the appropriate translation table and returns the number
//  of byte characters written to pMBStr.  This routine is only called if
//  the defaultcheck and compositecheck flags were both set.
//
//  NOTE:  Most significant bit of dwFlags parameter is used by this routine
//         to indicate that the caller only wants the count of the number of
//         characters written, not the string (ie. do not back up in buffer).
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetMBCompSB(
	PCP_HASH pHashN,
	DWORD dwFlags,
	LPWSTR pWCStr,
	LPBYTE pMBStr,
	int mbCount,
	WORD wDefault,
	LPBOOL pUsedDef)
{
	WCHAR PreComp;                // precomposed wide character


	if (!IS_NONSPACE_ONLY(pTblPtrs->pDefaultSortkey, *pWCStr))
	{
		//
		//  Get the 1:1 translation from wide char to single byte.
		//
		GET_WC_TRANSLATION_SB(pHashN,
			*pWCStr,
			pMBStr,
			wDefault,
			pUsedDef,
			dwFlags);
		return (1);
	}
	else
	{
		if (mbCount < 1)
		{
			//
			//  Need to handle the nonspace character by itself, since
			//  it is the first character in the string.
			//
			if (dwFlags & WC_DISCARDNS)
			{
				//
				//  Discard the non-spacing char, so just return with
				//  zero chars written.
				//
				return (0);
			}
			else if (dwFlags & WC_DEFAULTCHAR)
			{
				//
				//  Need to replace the nonspace character with the default
				//  character and return the number of characters written
				//  to the multibyte string.
				//
				*pUsedDef = TRUE;
				*pMBStr = LOBYTE(wDefault);
				return (1);
			}
			else                  // WC_SEPCHARS - default
			{
				//
				//  Get the 1:1 translation from wide char to multibyte
				//  of the non-spacing char and return the number of
				//  characters written to the multibyte string.
				//
				GET_WC_TRANSLATION_SB(pHashN,
					*pWCStr,
					pMBStr,
					wDefault,
					pUsedDef,
					dwFlags);
				return (1);
			}
		}
		else if (PreComp = GetPreComposedChar(*pWCStr, *(pWCStr - 1)))
		{
			//
			//  Back up in the single byte string and write the
			//  precomposed char.
			//
			if (!IS_MSB(dwFlags))
			{
				pMBStr--;
			}

			GET_WC_TRANSLATION_SB(pHashN,
				PreComp,
				pMBStr,
				wDefault,
				pUsedDef,
				dwFlags);
			return (0);
		}
		else
		{
			if (dwFlags & WC_DISCARDNS)
			{
				//
				//  Discard the non-spacing char, so just return with
				//  zero chars written.
				//
				return (0);
			}
			else if (dwFlags & WC_DEFAULTCHAR)
			{
				//
				//  Need to replace the base character with the default
				//  character.  Since we've already written the base
				//  translation char in the single byte string, we need to
				//  back up in the single byte string and write the default
				//  char.
				//
				if (!IS_MSB(dwFlags))
				{
					pMBStr--;
				}

				*pUsedDef = TRUE;
				*pMBStr = LOBYTE(wDefault);
				return (0);
			}
			else                  // WC_SEPCHARS - default
			{
				//
				//  Get the 1:1 translation from wide char to multibyte
				//  of the non-spacing char and return the number of
				//  characters written to the multibyte string.
				//
				GET_WC_TRANSLATION_SB(pHashN,
					*pWCStr,
					pMBStr,
					wDefault,
					pUsedDef,
					dwFlags);
				return (1);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
//
//  GetMBCompMB
//
//  Fills in pMBStr with the byte character(s) for the corresponding wide
//  character from the appropriate translation table and returns the number
//  of byte characters written to pMBStr.  This routine is only called if
//  the defaultcheck and compositecheck flags were both set.
//
//  If the buffer was too small, the fError flag will be set to TRUE.
//
//  NOTE:  Most significant bit of dwFlags parameter is used by this routine
//         to indicate that the caller only wants the count of the number of
//         characters written, not the string (ie. do not back up in buffer).
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetMBCompMB(
	PCP_HASH pHashN,
	DWORD dwFlags,
	LPWSTR pWCStr,
	LPBYTE pMBStr,
	int mbCount,
	WORD wDefault,
	LPBOOL pUsedDef,
	BOOL *fError,
	BOOL fOnlyOne)
{
	WCHAR PreComp;                // precomposed wide character
	BYTE pTmpSp[2];               // temp space - 2 bytes for DBCS
	int nCnt;                     // number of characters written


	*fError = FALSE;
	if (!IS_NONSPACE_ONLY(pTblPtrs->pDefaultSortkey, *pWCStr))
	{
		//
		//  Get the 1:1 translation from wide char to multibyte.
		//  This also handles DBCS and returns the number of characters
		//  written to the multibyte string.
		//
		GET_WC_TRANSLATION_MB(pHashN,
			*pWCStr,
			pMBStr,
			wDefault,
			pUsedDef,
			nCnt,
			fOnlyOne,
			dwFlags);
		if (nCnt == 0)
		{
			*fError = TRUE;
		}
		return (nCnt);
	}
	else
	{
		if (mbCount < 1)
		{
			//
			//  Need to handle the nonspace character by itself, since
			//  it is the first character in the string.
			//
			if (dwFlags & WC_DISCARDNS)
			{
				//
				//  Discard the non-spacing char, so just return with
				//  zero chars written.
				//
				return (0);
			}
			else if (dwFlags & WC_DEFAULTCHAR)
			{
				//
				//  Need to replace the nonspace character with the default
				//  character and return the number of characters written
				//  to the multibyte string.
				//
				*pUsedDef = TRUE;
				COPY_MB_CHAR(wDefault,
					pMBStr,
					nCnt,
					fOnlyOne);
				if (nCnt == 0)
				{
					*fError = TRUE;
				}
				return (nCnt);
			}
			else                  // WC_SEPCHARS - default
			{
				//
				//  Get the 1:1 translation from wide char to multibyte
				//  of the non-spacing char and return the number of
				//  characters written to the multibyte string.
				//
				GET_WC_TRANSLATION_MB(pHashN,
					*pWCStr,
					pMBStr,
					wDefault,
					pUsedDef,
					nCnt,
					fOnlyOne,
					dwFlags);
				if (nCnt == 0)
				{
					*fError = TRUE;
				}
				return (nCnt);
			}

		}
		else if (PreComp = GetPreComposedChar(*pWCStr, *(pWCStr - 1)))
		{
			//
			//  Get the 1:1 translation from wide char to multibyte
			//  of the precomposed char, back up in the multibyte string,
			//  write the precomposed char, and return the DIFFERENCE of
			//  the number of characters written to the the multibyte
			//  string.
			//
			GET_WC_TRANSLATION_MB(pHashN,
				*(pWCStr - 1),
				pTmpSp,
				wDefault,
				pUsedDef,
				nCnt,
				fOnlyOne,
				dwFlags);
			if (nCnt == 0)
			{
				*fError = TRUE;
				return (nCnt);
			}

			if (!IS_MSB(dwFlags))
			{
				pMBStr -= nCnt;
			}

			GET_WC_TRANSLATION_MB(pHashN,
				PreComp,
				pMBStr,
				wDefault,
				pUsedDef,
				mbCount,
				fOnlyOne,
				dwFlags);
			if (mbCount == 0)
			{
				*fError = TRUE;
			}
			return (mbCount - nCnt);
		}
		else
		{
			if (dwFlags & WC_DISCARDNS)
			{
				//
				//  Discard the non-spacing char, so just return with
				//  zero chars written.
				//
				return (0);
			}
			else if (dwFlags & WC_DEFAULTCHAR)
			{
				//
				//  Need to replace the base character with the default
				//  character.  Since we've already written the base
				//  translation char in the multibyte string, we need to
				//  back up in the multibyte string and return the
				//  DIFFERENCE of the number of characters written
				//  (could be negative).
				//

				//
				//  If the previous character written is the default
				//  character, then the base character for this nonspace
				//  character has already been replaced.  Simply throw
				//  this character away and return zero chars written.
				//
				if (!IS_MSB(dwFlags))
				{
					//
					//  Not using a temporary buffer, so find out if the
					//  previous character translated was the default char.
					//
					if ((MAKEWORD(*(pMBStr - 1), 0) == wDefault) ||
						((mbCount > 1) &&
						(MAKEWORD(*(pMBStr - 1), *(pMBStr - 2)) == wDefault)))
					{
						return (0);
					}
				}
				else
				{
					//
					//  Using a temporary buffer.  The temp buffer is 2 bytes
					//  in length and contains the previous character written.
					//
					if ((MAKEWORD(*pMBStr, 0) == wDefault) ||
						((mbCount > 1) &&
						(MAKEWORD(*pMBStr, *(pMBStr + 1)) == wDefault)))
					{
						return (0);
					}
				}

				//
				//  Get the 1:1 translation from wide char to multibyte
				//  of the base char, back up in the multibyte string,
				//  write the default char, and return the DIFFERENCE of
				//  the number of characters written to the the multibyte
				//  string.
				//
				GET_WC_TRANSLATION_MB(pHashN,
					*(pWCStr - 1),
					pTmpSp,
					wDefault,
					pUsedDef,
					nCnt,
					fOnlyOne,
					dwFlags);
				if (nCnt == 0)
				{
					*fError = TRUE;
					return (nCnt);
				}

				if (!IS_MSB(dwFlags))
				{
					pMBStr -= nCnt;
				}

				*pUsedDef = TRUE;
				COPY_MB_CHAR(wDefault,
					pMBStr,
					mbCount,
					fOnlyOne);
				if (mbCount == 0)
				{
					*fError = TRUE;
				}
				return (mbCount - nCnt);
			}
			else                  // WC_SEPCHARS - default
			{
				//
				//  Get the 1:1 translation from wide char to multibyte
				//  of the non-spacing char and return the number of
				//  characters written to the multibyte string.
				//
				GET_WC_TRANSLATION_MB(pHashN,
					*pWCStr,
					pMBStr,
					wDefault,
					pUsedDef,
					nCnt,
					fOnlyOne,
					dwFlags);
				if (nCnt == 0)
				{
					*fError = TRUE;
				}
				return (nCnt);
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////
//
//  GetMBDefaultComp
//
//  Translates the wide character string to a multibyte string and returns
//  the number of bytes written.  This also checks for the use of the default
//  character and tries to convert composite forms to precomposed forms, so
//  the translation is a lot slower.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetMBDefaultComp(
	PCP_HASH pHashN,
	LPWSTR pWCStr,
	LPWSTR pEndWCStr,
	LPBYTE pMBStr,
	int cbMultiByte,
	WORD wDefault,
	LPBOOL pUsedDef,
	DWORD dwFlags)
{
	int mbIncr;                   // amount to increment pMBStr
	int mbCount = 0;              // count of multibyte chars written
	LPBYTE pEndMBStr;             // ptr to end of MB string buffer
	BOOL fError;                  // if error during MB conversion


	//
	//  If cbMultiByte is 0, then we can't use pMBStr.  In this
	//  case, we simply want to count the number of characters that
	//  would be written to the buffer.
	//
	if (cbMultiByte == 0)
	{
		BYTE pTempStr[2];             // tmp buffer - 2 bytes for DBCS

		//
		//  Set most significant bit of flags to indicate to the
		//  GetMBComp routine that it's using a temporary storage
		//  area, so don't back up in the buffer.
		//
		SET_MSB(dwFlags);

		//
		//  For each wide char, translate it to its corresponding multibyte
		//  char and increment the multibyte character count.
		//
		if (IS_SBCS_CP(pHashN))
		{
			//
			//  Single Byte Character Code Page.
			//
			while (pWCStr < pEndWCStr)
			{
				//
				//  Get the translation.
				//
				mbCount += GetMBCompSB(pHashN,
					dwFlags,
					pWCStr,
					pTempStr,
					mbCount,
					wDefault,
					pUsedDef);
				pWCStr++;
			}
		}
		else
		{
			//
			//  Multi Byte Character Code Page.
			//
			while (pWCStr < pEndWCStr)
			{
				//
				//  Get the translation.
				//
				mbCount += GetMBCompMB(pHashN,
					dwFlags,
					pWCStr,
					pTempStr,
					mbCount,
					wDefault,
					pUsedDef,
					&fError,
					FALSE);
				pWCStr++;
			}
		}
	}
	else
	{
		//
		//  Initialize multibyte loop pointers.
		//
		pEndMBStr = pMBStr + cbMultiByte;

		//
		//  For each wide char, translate it to its corresponding
		//  multibyte char, store it in pMBStr, and increment the
		//  multibyte character count.
		//
		if (IS_SBCS_CP(pHashN))
		{
			//
			//  Single Byte Character Code Page.
			//
			while ((pWCStr < pEndWCStr) && (pMBStr < pEndMBStr))
			{
				//
				//  Get the translation.
				//
				mbIncr = GetMBCompSB(pHashN,
					dwFlags,
					pWCStr,
					pMBStr,
					mbCount,
					wDefault,
					pUsedDef);
				pWCStr++;
				mbCount += mbIncr;
				pMBStr += mbIncr;
			}
		}
		else
		{
			//
			//  Multi Byte Character Code Page.
			//
			while ((pWCStr < pEndWCStr) && (pMBStr < pEndMBStr))
			{
				//
				//  Get the translation.
				//
				mbIncr = GetMBCompMB(pHashN,
					dwFlags,
					pWCStr,
					pMBStr,
					mbCount,
					wDefault,
					pUsedDef,
					&fError,
					((pMBStr + 1) < pEndMBStr) ? FALSE : TRUE);
				if (fError)
				{
					//
					//  Not enough room in the buffer.
					//
					break;
				}

				pWCStr++;
				mbCount += mbIncr;
				pMBStr += mbIncr;
			}
		}

		//
		//  Make sure multibyte character buffer was large enough.
		//
		if (pWCStr < pEndWCStr)
		{
			(ERROR_INSUFFICIENT_BUFFER);
			return (0);
		}
	}

	//
	//  Return the number of characters written (or that would have
	//  been written) to the buffer.
	//
	return (mbCount);
}


PCP_HASH __fastcall GetCPHashNode(
	UINT CodePage)
{
	PCP_HASH pHashN;              // ptr to CP hash node


	//
	//  Get hash node.
	//
	FIND_CP_HASH_NODE(CodePage, pHashN);

	//
	//  If the hash node does not exist, try to get the tables
	//  from the appropriate data file.
	//
	//  NOTE:  No need to check error code from GetCodePageFileInfo,
	//         because pHashN is not touched if there was an
	//         error.  Thus, pHashN will still be NULL, and an
	//         "error" will be returned from this routine.
	//
	if (pHashN == NULL)
	{
		//
		//  Hash node does NOT exist.
		//
		//RtlEnterCriticalSection(&gcsTblPtrs);
		FIND_CP_HASH_NODE(CodePage, pHashN);
		if (pHashN == NULL)
		{
			//GetCodePageFileInfo(CodePage, &pHashN);
		}
		//	RtlLeaveCriticalSection(&gcsTblPtrs);
	}

	//
	//  Return pointer to hash node.
	//
	return (pHashN);
}
int __fastcall NlsStrLenW(
	LPCWSTR pwsz)
{
	LPCWSTR pwszStart = pwsz;          // ptr to beginning of string

loop:
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;
	if (!*pwsz)    goto  done;
	pwsz++;

	goto  loop;

done:
	return ((int)(pwsz - pwszStart));
}

int UnicodeToUTF7(
	LPCWSTR lpSrcStr,
	int cchSrc,
	LPSTR lpDestStr,
	int cchDest)
{
	LPCWSTR lpWC = lpSrcStr;
	BOOL fShift = FALSE;
	DWORD dwBit = 0;              // 32-bit buffer
	int iPos = 0;                 // 6-bit position in buffer
	int cchU7 = 0;                // # of UTF7 chars generated


	while ((cchSrc--) && ((cchDest == 0) || (cchU7 < cchDest)))
	{
		if ((*lpWC > ASCII) || (fShiftChar[*lpWC]))
		{
			//
			//  Need shift.  Store 16 bits in buffer.
			//
			dwBit |= ((DWORD)*lpWC) << (16 - iPos);
			iPos += 16;

			if (!fShift)
			{
				//
				//  Not in shift state, so add "+".
				//
				if (cchDest)
				{
					lpDestStr[cchU7] = SHIFT_IN;
				}
				cchU7++;

				//
				//  Go into shift state.
				//
				fShift = TRUE;
			}

			//
			//  Output 6 bits at a time as Base64 chars.
			//
			while (iPos >= 6)
			{
				if (cchDest)
				{
					if (cchU7 < cchDest)
					{
						//
						//  26 = 32 - 6
						//
						lpDestStr[cchU7] = cBase64[(int)(dwBit >> 26)];
					}
					else
					{
						break;
					}
				}

				cchU7++;
				dwBit <<= 6;           // remove from bit buffer
				iPos -= 6;             // adjust position pointer
			}
			if (iPos >= 6)
			{
				//
				//  Error - buffer too small.
				//
				cchSrc++;
				break;
			}
		}
		else
		{
			//
			//  No need to shift.
			//
			if (fShift)
			{
				//
				//  End the shift sequence.
				//
				fShift = FALSE;

				if (iPos != 0)
				{
					//
					//  Some bits left in dwBit.
					//
					if (cchDest)
					{
						if ((cchU7 + 1) < cchDest)
						{
							lpDestStr[cchU7++] = cBase64[(int)(dwBit >> 26)];
							lpDestStr[cchU7++] = SHIFT_OUT;
						}
						else
						{
							//
							//  Error - buffer too small.
							//
							cchSrc++;
							break;
						}
					}
					else
					{
						cchU7 += 2;
					}

					dwBit = 0;         // reset bit buffer
					iPos = 0;         // reset postion pointer
				}
				else
				{
					//
					//  Simply end the shift sequence.
					//
					if (cchDest)
					{
						lpDestStr[cchU7++] = SHIFT_OUT;
					}
					else
					{
						cchU7++;
					}
				}
			}

			//
			//  Write the character to the buffer.
			//  If the character is "+", then write "+-".
			//
			if (cchDest)
			{
				if (cchU7 < cchDest)
				{
					lpDestStr[cchU7++] = (char)*lpWC;

					if (*lpWC == SHIFT_IN)
					{
						if (cchU7 < cchDest)
						{
							lpDestStr[cchU7++] = SHIFT_OUT;
						}
						else
						{
							//
							//  Error - buffer too small.
							//
							cchSrc++;
							break;
						}
					}
				}
				else
				{
					//
					//  Error - buffer too small.
					//
					cchSrc++;
					break;
				}
			}
			else
			{
				cchU7++;

				if (*lpWC == SHIFT_IN)
				{
					cchU7++;
				}
			}
		}

		lpWC++;
	}

	//
	//  See if we're still in the shift state.
	//
	if (fShift)
	{
		if (iPos != 0)
		{
			//
			//  Some bits left in dwBit.
			//
			if (cchDest)
			{
				if ((cchU7 + 1) < cchDest)
				{
					lpDestStr[cchU7++] = cBase64[(int)(dwBit >> 26)];
					lpDestStr[cchU7++] = SHIFT_OUT;
				}
				else
				{
					//
					//  Error - buffer too small.
					//
					cchSrc++;
				}
			}
			else
			{
				cchU7 += 2;
			}
		}
		else
		{
			//
			//  Simply end the shift sequence.
			//
			if (cchDest)
			{
				lpDestStr[cchU7++] = SHIFT_OUT;
			}
			else
			{
				cchU7++;
			}
		}
	}

	//
	//  Make sure the destination buffer was large enough.
	//
	if (cchDest && (cchSrc >= 0))
	{
		(ERROR_INSUFFICIENT_BUFFER);
		return (0);
	}

	//
	//  Return the number of UTF-7 characters written.
	//
	return (cchU7);
}


////////////////////////////////////////////////////////////////////////////
//
//  UnicodeToUTF8
//
//  Maps a Unicode character string to its UTF-8 string counterpart.
//
//  02-06-96    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int UnicodeToUTF8(
	LPCWSTR lpSrcStr,
	int cchSrc,
	LPSTR lpDestStr,
	int cchDest)
{
	LPCWSTR lpWC = lpSrcStr;
	int     cchU8 = 0;                // # of UTF8 chars generated
	DWORD   dwSurrogateChar;
	WCHAR   wchHighSurrogate = 0;
	BOOL    bHandled;


	while ((cchSrc--) && ((cchDest == 0) || (cchU8 < cchDest)))
	{
		bHandled = FALSE;

		//
		// Check if high surrogate is available
		//
		if ((*lpWC >= HIGH_SURROGATE_START) && (*lpWC <= HIGH_SURROGATE_END))
		{
			if (cchDest)
			{
				// Another high surrogate, then treat the 1st as normal
				// Unicode character.
				if (wchHighSurrogate)
				{
					if ((cchU8 + 2) < cchDest)
					{
						lpDestStr[cchU8++] = (CHAR)(UTF8_1ST_OF_3 | HIGHER_6_BIT(wchHighSurrogate));
						lpDestStr[cchU8++] = (CHAR)(UTF8_TRAIL | MIDDLE_6_BIT(wchHighSurrogate));
						lpDestStr[cchU8++] = (CHAR)(UTF8_TRAIL | LOWER_6_BIT(wchHighSurrogate));
					}
					else
					{
						// not enough buffer
						cchSrc++;
						break;
					}
				}
			}
			else
			{
				cchU8 += 3;
			}
			wchHighSurrogate = *lpWC;
			bHandled = TRUE;
		}

		if (!bHandled && wchHighSurrogate)
		{
			if ((*lpWC >= LOW_SURROGATE_START) && (*lpWC <= LOW_SURROGATE_END))
			{
				// wheee, valid surrogate pairs

				if (cchDest)
				{
					if ((cchU8 + 3) < cchDest)
					{
						dwSurrogateChar = (((wchHighSurrogate - 0xD800) << 10) + (*lpWC - 0xDC00) + 0x10000);

						lpDestStr[cchU8++] = (UTF8_1ST_OF_4 |
							(unsigned char)(dwSurrogateChar >> 18));           // 3 bits from 1st byte

						lpDestStr[cchU8++] = (UTF8_TRAIL |
							(unsigned char)((dwSurrogateChar >> 12) & 0x3f)); // 6 bits from 2nd byte

						lpDestStr[cchU8++] = (UTF8_TRAIL |
							(unsigned char)((dwSurrogateChar >> 6) & 0x3f));   // 6 bits from 3rd byte

						lpDestStr[cchU8++] = (UTF8_TRAIL |
							(unsigned char)(0x3f & dwSurrogateChar));          // 6 bits from 4th byte
					}
					else
					{
						// not enough buffer
						cchSrc++;
						break;
					}
				}
				else
				{
					// we already counted 3 previously (in high surrogate)
					cchU8 += 1;
				}

				bHandled = TRUE;
			}
			else
			{
				// Bad Surrogate pair : ERROR
				// Just process wchHighSurrogate , and the code below will
				// process the current code point
				if (cchDest)
				{
					if ((cchU8 + 2) < cchDest)
					{
						lpDestStr[cchU8++] = (CHAR)(UTF8_1ST_OF_3 | HIGHER_6_BIT(wchHighSurrogate));
						lpDestStr[cchU8++] = (CHAR)(UTF8_TRAIL | MIDDLE_6_BIT(wchHighSurrogate));
						lpDestStr[cchU8++] = (CHAR)(UTF8_TRAIL | LOWER_6_BIT(wchHighSurrogate));
					}
					else
					{
						// not enough buffer
						cchSrc++;
						break;
					}
				}
			}

			wchHighSurrogate = 0;
		}

		if (!bHandled)
		{
			if (*lpWC <= ASCII)
			{
				//
				//  Found ASCII.
				//
				if (cchDest)
				{
					lpDestStr[cchU8] = (char)*lpWC;
				}
				cchU8++;
			}
			else if (*lpWC <= UTF8_2_MAX)
			{
				//
				//  Found 2 byte sequence if < 0x07ff (11 bits).
				//
				if (cchDest)
				{
					if ((cchU8 + 1) < cchDest)
					{
						//
						//  Use upper 5 bits in first byte.
						//  Use lower 6 bits in second byte.
						//
						lpDestStr[cchU8++] = (CHAR)(UTF8_1ST_OF_2 | (*lpWC >> 6));
						lpDestStr[cchU8++] = (CHAR)(UTF8_TRAIL | LOWER_6_BIT(*lpWC));
					}
					else
					{
						//
						//  Error - buffer too small.
						//
						cchSrc++;
						break;
					}
				}
				else
				{
					cchU8 += 2;
				}
			}
			else
			{
				//
				//  Found 3 byte sequence.
				//
				if (cchDest)
				{
					if ((cchU8 + 2) < cchDest)
					{
						//
						//  Use upper  4 bits in first byte.
						//  Use middle 6 bits in second byte.
						//  Use lower  6 bits in third byte.
						//
						lpDestStr[cchU8++] = (CHAR)(UTF8_1ST_OF_3 | HIGHER_6_BIT(*lpWC));
						lpDestStr[cchU8++] = (CHAR)(UTF8_TRAIL | MIDDLE_6_BIT(*lpWC));
						lpDestStr[cchU8++] = UTF8_TRAIL | LOWER_6_BIT(*lpWC);
					}
					else
					{
						//
						//  Error - buffer too small.
						//
						cchSrc++;
						break;
					}
				}
				else
				{
					cchU8 += 3;
				}
			}
		}

		lpWC++;
	}

	//
	// If the last character was a high surrogate, then handle it as a normal
	// unicode character.
	//
	if ((cchSrc < 0) && (wchHighSurrogate != 0))
	{
		if (cchDest)
		{
			if ((cchU8 + 2) < cchDest)
			{
				lpDestStr[cchU8++] = UTF8_1ST_OF_3 | HIGHER_6_BIT(wchHighSurrogate);
				lpDestStr[cchU8++] = (CHAR)(UTF8_TRAIL | MIDDLE_6_BIT(wchHighSurrogate));
				lpDestStr[cchU8++] = UTF8_TRAIL | LOWER_6_BIT(wchHighSurrogate);
			}
			else
			{
				cchSrc++;
			}
		}
	}

	//
	//  Make sure the destination buffer was large enough.
	//
	if (cchDest && (cchSrc >= 0))
	{
		(ERROR_INSUFFICIENT_BUFFER);
		return (0);
	}

	//
	//  Return the number of UTF-8 characters written.
	//
	return (cchU8);
}
int UnicodeToUTF(
	UINT CodePage,
	DWORD dwFlags,
	LPCWSTR lpWideCharStr,
	int cchWideChar,
	LPSTR lpMultiByteStr,
	int cbMultiByte,
	LPCSTR lpDefaultChar,
	LPBOOL lpUsedDefaultChar)
{
	int rc = 0;


	//
	//  Invalid Parameter Check:
	//     - validate code page
	//     - length of WC string is 0
	//     - multibyte buffer size is negative
	//     - WC string is NULL
	//     - length of WC string is NOT zero AND
	//         (MB string is NULL OR src and dest pointers equal)
	//     - lpDefaultChar and lpUsedDefaultChar not NULL
	//
	if ((CodePage < CP_UTF7) || (CodePage > CP_UTF8) ||
		(cchWideChar == 0) || (cbMultiByte < 0) ||
		(lpWideCharStr == NULL) ||
		((cbMultiByte != 0) &&
		((lpMultiByteStr == NULL) ||
			(lpWideCharStr == (LPWSTR)lpMultiByteStr))) ||
			(lpDefaultChar != NULL) || (lpUsedDefaultChar != NULL))
	{
		(ERROR_INVALID_PARAMETER);
		return (0);
	}

	//
	//  Invalid Flags Check:
	//     - flags not 0
	//
	if (dwFlags != 0)
	{
		(ERROR_INVALID_FLAGS);
		return (0);
	}

	//
	//  If cchWideChar is -1, then the string is null terminated and we
	//  need to get the length of the string.  Add one to the length to
	//  include the null termination.  (This will always be at least 1.)
	//
	if (cchWideChar <= -1)
	{
		cchWideChar = NlsStrLenW(lpWideCharStr) + 1;
	}

	switch (CodePage)
	{
	case (CP_UTF7):
	{
		rc = UnicodeToUTF7(lpWideCharStr,
			cchWideChar,
			lpMultiByteStr,
			cbMultiByte);
		break;
	}
	case (CP_UTF8):
	{
		rc = UnicodeToUTF8(lpWideCharStr,
			cchWideChar,
			lpMultiByteStr,
			cbMultiByte);
		break;
	}
	}

	return (rc);
}


#define GET_CP_HASH_NODE( CodePage,                                        \
                          pHashN )                                         \
{                                                                          \
					                                                       \
                                                                           \
                                                                           \
    /*                                                                     \
     *  Check for the ACP, OEMCP, or MACCP.  Fill in the appropriate       \
     *  value for the code page if one of these values is given.           \
     *  Otherwise, just get the hash node for the given code page.         \
     */                                                                    \
    if (CodePage == gAnsiCodePage)                                         \
    {                                                                      \
        pHashN = gpACPHashN;                                               \
    }                                                                      \
    else if (CodePage == gOemCodePage)                                     \
    {                                                                      \
        pHashN = gpOEMCPHashN;                                             \
    }                                                                      \
    else if (CodePage == CP_ACP)                                           \
    {                                                                      \
        CodePage = gAnsiCodePage;                                          \
        pHashN = gpACPHashN;                                               \
    }                                                                      \
    else if (CodePage == CP_OEMCP)                                         \
    {                                                                      \
        CodePage = gOemCodePage;                                           \
        pHashN = gpOEMCPHashN;                                             \
    }                                                                      \
    else                                                                   \
    {                                                                      \
        pHashN = GetCPHashNode(CodePage);                                  \
    }                                                                      \
}

int WINAPI impl_WideCharToMultiByte(
	UINT CodePage,
	DWORD dwFlags,
	LPCWSTR lpWideCharStr,
	int cchWideChar,
	LPSTR lpMultiByteStr,
	int cbMultiByte,
	LPCSTR lpDefaultChar,
	LPBOOL lpUsedDefaultChar)
{
	PCP_HASH pHashN;              // ptr to CP hash node
	LPWSTR pWCStr;                // ptr to search through WC string
	LPWSTR pEndWCStr;             // ptr to end of WC string buffer
	WORD wDefault = 0;            // default character as a word
	int IfNoDefault;              // if default check is to be made
	int IfCompositeChk;           // if check for composite
	BOOL TmpUsed;                 // temp storage for default used
	int ctr;                      // loop counter


	//
	//  See if it's a special code page value for UTF translations.
	//
	if (CodePage >= NLS_CP_ALGORITHM_RANGE)
	{
		return (UnicodeToUTF(CodePage,
			dwFlags,
			lpWideCharStr,
			cchWideChar,
			lpMultiByteStr,
			cbMultiByte,
			lpDefaultChar,
			lpUsedDefaultChar));
	}

	//
	//  Get the code page value and the appropriate hash node.
	//
	GET_CP_HASH_NODE(CodePage, pHashN);



	//
	//  Invalid Parameter Check:
	//     - length of WC string is 0
	//     - multibyte buffer size is negative
	//     - WC string is NULL
	//     - length of WC string is NOT zero AND
	//         (MB string is NULL OR src and dest pointers equal)
	//
	if ((cchWideChar == 0) || (cbMultiByte < 0) ||
		(lpWideCharStr == NULL) ||
		((cbMultiByte != 0) &&
		((lpMultiByteStr == NULL) ||
			(lpWideCharStr == (LPWSTR)lpMultiByteStr))))
	{
		(ERROR_INVALID_PARAMETER);
		return (0);
	}

	//
	//  If cchWideChar is -1, then the string is null terminated and we
	//  need to get the length of the string.  Add one to the length to
	//  include the null termination.  (This will always be at least 1.)
	//
	if (cchWideChar <= -1)
	{
		cchWideChar = NlsStrLenW(lpWideCharStr) + 1;
	}

	//
	//  Check for valid code page.
	//
	if (pHashN == NULL)
	{
		//
		//  Special case the CP_SYMBOL code page.
		//
		if ((CodePage == CP_SYMBOL) && (dwFlags == 0) &&
			(lpDefaultChar == NULL) && (lpUsedDefaultChar == NULL))
		{
			//
			//  If the caller just wants the size of the buffer needed
			//  to do this translation, return the size of the MB string.
			//
			if (cbMultiByte == 0)
			{
				return (cchWideChar);
			}

			//
			//  Make sure the buffer is large enough.
			//
			if (cbMultiByte < cchWideChar)
			{
				(ERROR_INSUFFICIENT_BUFFER);
				return (0);
			}

			//
			//  Translate Unicode char f0xx to SB xx.
			//    0x0000->0x001f map to 0x00->0x1f
			//    0xf020->0xf0ff map to 0x20->0xff
			//
			for (ctr = 0; ctr < cchWideChar; ctr++)
			{
				if ((lpWideCharStr[ctr] >= 0x0020) &&
					((lpWideCharStr[ctr] < 0xf020) ||
					(lpWideCharStr[ctr] > 0xf0ff)))
				{
					(ERROR_NO_UNICODE_TRANSLATION);                        \
						return (0);
				}
				lpMultiByteStr[ctr] = (BYTE)lpWideCharStr[ctr];
			}
			return (cchWideChar);
		}
		else
		{
			(((CodePage == CP_SYMBOL) && (dwFlags != 0))
				? ERROR_INVALID_FLAGS
				: ERROR_INVALID_PARAMETER);
			return (0);
		}
	}

	//
	//  See if the given code page is in the DLL range.
	//
	if (pHashN->pfnCPProc)
	{
		//
		//  Invalid Parameter Check:
		//     - lpDefaultChar not NULL
		//     - lpUsedDefaultChar not NULL
		//
		if ((lpDefaultChar != NULL) || (lpUsedDefaultChar != NULL))
		{
			(ERROR_INVALID_PARAMETER);
			return (0);
		}

		//
		//  Invalid Flags Check:
		//     - flags not 0
		//
		if (dwFlags != 0)
		{
			(ERROR_INVALID_FLAGS);
			return (0);
		}

		//
		//  Call the DLL to do the translation.
		//
		return ((*(pHashN->pfnCPProc))(CodePage,
			NLS_CP_WCTOMB,
			(LPSTR)lpMultiByteStr,
			cbMultiByte,
			(LPWSTR)lpWideCharStr,
			cchWideChar,
			NULL));
	}

	//
	//  Invalid Flags Check:
	//     - compositechk flag is not set AND any of comp flags are set
	//     - flags other than valid ones
	//
	if (((!(IfCompositeChk = (dwFlags & WC_COMPOSITECHECK))) &&
		(dwFlags & WC_COMPCHK_FLAGS)) ||
		(dwFlags & WC_INVALID_FLAG))
	{
		(ERROR_INVALID_FLAGS);
		return (0);
	}

	//
	//  Initialize wide character loop pointers.
	//
	pWCStr = (LPWSTR)lpWideCharStr;
	pEndWCStr = pWCStr + cchWideChar;

	//
	//  Set the IfNoDefault parameter to TRUE if both lpDefaultChar and
	//  lpUsedDefaultChar are NULL.
	//
	IfNoDefault = ((lpDefaultChar == NULL) && (lpUsedDefaultChar == NULL));

	//
	//  If the composite check flag is NOT set AND both of the default
	//  parameters (lpDefaultChar and lpUsedDefaultChar) are null, then
	//  do the quick translation.
	//
	if (IfNoDefault && !IfCompositeChk)
	{
		//
		//  Translate WC string to MB string, ignoring default chars.
		//
		return (GetMBNoDefault(pHashN,
			pWCStr,
			pEndWCStr,
			(LPBYTE)lpMultiByteStr,
			cbMultiByte,
			dwFlags));
	}

	//
	//  Set the system default character.
	//
	wDefault = pHashN->pCPInfo->wDefaultChar;

	//
	//  See if the default check is needed.
	//
	if (!IfNoDefault)
	{
		//
		//  If lpDefaultChar is NULL, then use the system default.
		//  Form a word out of the default character.  Single byte
		//  characters are zero extended, DBCS characters are as is.
		//
		if (lpDefaultChar != NULL)
		{
			wDefault = GET_DEFAULT_WORD(pHashN->pDBCSOffsets,
				(LPBYTE)lpDefaultChar);
		}

		//
		//  If lpUsedDefaultChar is NULL, then it won't be used later
		//  on if a default character is detected.  Otherwise, we need
		//  to initialize it.
		//
		if (lpUsedDefaultChar == NULL)
		{
			lpUsedDefaultChar = &TmpUsed;
		}
		*lpUsedDefaultChar = FALSE;

		//
		//  Check for "composite check" flag.
		//
		if (!IfCompositeChk)
		{
			//
			//  Translate WC string to MB string, checking for the use of the
			//  default character.
			//
			return (GetMBDefault(pHashN,
				pWCStr,
				pEndWCStr,
				(LPBYTE)lpMultiByteStr,
				cbMultiByte,
				wDefault,
				lpUsedDefaultChar,
				dwFlags));
		}
		else
		{
			//
			//  Translate WC string to MB string, checking for the use of the
			//  default character.
			//
			return (GetMBDefaultComp(pHashN,
				pWCStr,
				pEndWCStr,
				(LPBYTE)lpMultiByteStr,
				cbMultiByte,
				wDefault,
				lpUsedDefaultChar,
				dwFlags));
		}
	}
	else
	{
		//
		//  The only case left here is that the Composite check
		//  flag IS set and the default check flag is NOT set.
		//
		//  Translate WC string to MB string, checking for the use of the
		//  default character.
		//
		return (GetMBDefaultComp(pHashN,
			pWCStr,
			pEndWCStr,
			(LPBYTE)lpMultiByteStr,
			cbMultiByte,
			wDefault,
			&TmpUsed,
			dwFlags));
	}
}

#define CREATE_CODEPAGE_HASH_NODE( CodePage,                               \
                                   pHashN )                                \
{                                                                          \
    /*                                                                     \
     *  Allocate CP_HASH structure.                                        \
     */                                                                    \
pHashN = (PCP_HASH)malloc(sizeof(CP_HASH));								   \
																		   \
    if (pHashN == NULL)                                                   \
    {                                                                      \
        return (ERROR_OUTOFMEMORY);                                        \
    }                                                                      \
ZeroMemory(pHashN,sizeof(CP_HASH));										   \
                                                                           \
    /*                                                                     \
     *  Fill in the CodePage value.                                        \
     */                                                                    \
    pHashN->CodePage = CodePage;                                           \
                                                                           \
    /*                                                                     \
     *   Make sure the pfnCPProc value is NULL for now.                    \
     */                                                                    \
    pHashN->pfnCPProc = NULL;                                              \
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ULONG MakeCPHashNode(
	UINT CodePage,
	LPWORD pBaseAddr,
	PCP_HASH *ppNode,
	BOOL IsDLL,
	LPFN_CP_PROC pfnCPProc)
{
	PCP_HASH pHashN;                   // ptr to CP hash node
	WORD offMB;                        // offset to MB table
	WORD offWC;                        // offset to WC table
	PGLYPH_TABLE pGlyph;               // ptr to glyph table info
	PDBCS_RANGE pRange;                // ptr to DBCS range


	//
	//  Allocate CP_HASH structure and fill in the CodePage value.
	//
	CREATE_CODEPAGE_HASH_NODE(CodePage, pHashN);

	//
	//  See if we're dealing with a DLL or an NLS data file.
	//
	if (IsDLL)
	{
		if (pfnCPProc == NULL)
		{
			NLS_FREE_MEM(pHashN);
			return (ERROR_INVALID_PARAMETER);
		}

		pHashN->pfnCPProc = pfnCPProc;
	}
	else
	{
		//
		//  Get the offsets.
		//
		offMB = pBaseAddr[0];
		offWC = offMB + pBaseAddr[offMB];

		//
		//  Attach CP Info to CP hash node.
		//
		pHashN->pCPInfo = (PCP_TABLE)(pBaseAddr + CP_HEADER);

		//
		//  Attach MB table to CP hash node.
		//
		pHashN->pMBTbl = pBaseAddr + offMB + MB_HEADER;

		//
		//  Attach Glyph table to CP hash node (if it exists).
		//  Also, set the pointer to the DBCS ranges based on whether or
		//  not the GLYPH table is present.
		//
		pGlyph = pHashN->pMBTbl + MB_TBL_SIZE;
		if (pGlyph[0] != 0)
		{
			pHashN->pGlyphTbl = pGlyph + GLYPH_HEADER;
			pRange = pHashN->pDBCSRanges = pHashN->pGlyphTbl + GLYPH_TBL_SIZE;
		}
		else
		{
			pRange = pHashN->pDBCSRanges = pGlyph + GLYPH_HEADER;
		}

		//
		//  Attach DBCS information to CP hash node.
		//
		if (pRange[0] > 0)
		{
			//
			//  Set the pointer to the offsets section.
			//
			pHashN->pDBCSOffsets = pRange + DBCS_HEADER;
		}

		//
		//  Attach WC table to CP hash node.
		//
		pHashN->pWC = pBaseAddr + offWC + WC_HEADER;
	}

	//
	//  Save the pointer to the hash node.
	//
	if (ppNode != NULL)
	{
		*ppNode = pHashN;
	}

	//
	//  Return success.
	//
	return (NO_ERROR);
}

VOID InitMultiByteTranslation()
{
	LPWORD pBaseAddr;

	pBaseAddr = __GetPeb()->AnsiCodePageData;

	gAnsiCodePage = ((PCP_TABLE)(pBaseAddr + CP_HEADER))->CodePage;

	MakeCPHashNode(gAnsiCodePage,
		pBaseAddr,
		&gpACPHashN,
		FALSE,
		NULL);

	pBaseAddr = __GetPeb()->OemCodePageData;
	gOemCodePage = ((PCP_TABLE)(pBaseAddr + CP_HEADER))->CodePage;
	if (gOemCodePage != gAnsiCodePage) {

		MakeCPHashNode(gOemCodePage,
			pBaseAddr,
			&gpOEMCPHashN,
			FALSE,
			NULL);
	}
	else {

		gpOEMCPHashN = gpACPHashN;
	}

	/*
	pTblPtrs

	GetStringTypeW
	most called func -> GetCType -> first imm mov is _pTblPtrs

	or

	GetEraNameCountedString
	walk until call != GetNamedLocaleHashNode prev prev instructions -> mov eax, ds:_pTblPtrs
	*/
}


////////////////////////////////////////////////////////////////////////////
//
//  UTF7ToUnicode
//
//  Maps a UTF-7 character string to its wide character string counterpart.
//
//  02-06-96    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int UTF7ToUnicode(
	LPCSTR lpSrcStr,
	int cchSrc,
	LPWSTR lpDestStr,
	int cchDest)
{
	LPCSTR pUTF7 = lpSrcStr;
	BOOL fShift = FALSE;
	DWORD dwBit = 0;              // 32-bit buffer to hold temporary bits
	int iPos = 0;                 // 6-bit position pointer in the buffer
	int cchWC = 0;                // # of Unicode code points generated


	while ((cchSrc--) && ((cchDest == 0) || (cchWC < cchDest)))
	{
		if (*pUTF7 > ASCII)
		{
			//
			//  Error - non ASCII char, so zero extend it.
			//
			if (cchDest)
			{
				lpDestStr[cchWC] = (WCHAR)*pUTF7;
			}
			cchWC++;
		}
		else if (!fShift)
		{
			//
			//  Not in shifted sequence.
			//
			if (*pUTF7 == SHIFT_IN)
			{
				if (cchSrc && (pUTF7[1] == SHIFT_OUT))
				{
					//
					//  "+-" means "+"
					//
					if (cchDest)
					{
						lpDestStr[cchWC] = (WCHAR)*pUTF7;
					}
					pUTF7++;
					cchSrc--;
					cchWC++;
				}
				else
				{
					//
					//  Start a new shift sequence.
					//
					fShift = TRUE;
				}
			}
			else
			{
				//
				//  No need to shift.
				//
				if (cchDest)
				{
					lpDestStr[cchWC] = (WCHAR)*pUTF7;
				}
				cchWC++;
			}
		}
		else
		{
			//
			//  Already in shifted sequence.
			//
			if (nBitBase64[*pUTF7] == -1)
			{
				//
				//  Any non Base64 char also ends shift state.
				//
				if (*pUTF7 != SHIFT_OUT)
				{
					//
					//  Not "-", so write it to the buffer.
					//
					if (cchDest)
					{
						lpDestStr[cchWC] = (WCHAR)*pUTF7;
					}
					cchWC++;
				}

				//
				//  Reset bits.
				//
				fShift = FALSE;
				dwBit = 0;
				iPos = 0;
			}
			else
			{
				//
				//  Store the bits in the 6-bit buffer and adjust the
				//  position pointer.
				//
				dwBit |= ((DWORD)nBitBase64[*pUTF7]) << (26 - iPos);
				iPos += 6;
			}

			//
			//  Output the 16-bit Unicode value.
			//
			while (iPos >= 16)
			{
				if (cchDest)
				{
					if (cchWC < cchDest)
					{
						lpDestStr[cchWC] = (WCHAR)(dwBit >> 16);
					}
					else
					{
						break;
					}
				}
				cchWC++;

				dwBit <<= 16;
				iPos -= 16;
			}
			if (iPos >= 16)
			{
				//
				//  Error - buffer too small.
				//
				cchSrc++;
				break;
			}
		}

		pUTF7++;
	}

	//
	//  Make sure the destination buffer was large enough.
	//
	if (cchDest && (cchSrc >= 0))
	{
		(ERROR_INSUFFICIENT_BUFFER);
		return (0);
	}

	//
	//  Return the number of Unicode characters written.
	//
	return (cchWC);
}


////////////////////////////////////////////////////////////////////////////
//
//  UTF8ToUnicode
//
//  Maps a UTF-8 character string to its wide character string counterpart.
//
//  02-06-96    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int UTF8ToUnicode(
	LPCSTR lpSrcStr,
	int cchSrc,
	LPWSTR lpDestStr,
	int cchDest)
{
	int nTB = 0;                   // # trail bytes to follow
	int cchWC = 0;                 // # of Unicode code points generated
	LPCSTR pUTF8 = lpSrcStr;
	DWORD dwSurrogateChar;         // Full surrogate char
	BOOL bSurrogatePair = FALSE;   // Indicate we'r collecting a surrogate pair
	char UTF8;


	while ((cchSrc--) && ((cchDest == 0) || (cchWC < cchDest)))
	{
		//
		//  See if there are any trail bytes.
		//
		if (BIT7(*pUTF8) == 0)
		{
			//
			//  Found ASCII.
			//
			if (cchDest)
			{
				lpDestStr[cchWC] = (WCHAR)*pUTF8;
			}
			bSurrogatePair = FALSE;
			cchWC++;
		}
		else if (BIT6(*pUTF8) == 0)
		{
			//
			//  Found a trail byte.
			//  Note : Ignore the trail byte if there was no lead byte.
			//
			if (nTB != 0)
			{
				//
				//  Decrement the trail byte counter.
				//
				nTB--;

				if (bSurrogatePair)
				{
					dwSurrogateChar <<= 6;
					dwSurrogateChar |= LOWER_6_BIT(*pUTF8);

					if (nTB == 0)
					{
						if (cchDest)
						{
							if ((cchWC + 1) < cchDest)
							{
								lpDestStr[cchWC] = (WCHAR)
									(((dwSurrogateChar - 0x10000) >> 10) + HIGH_SURROGATE_START);

								lpDestStr[cchWC + 1] = (WCHAR)
									((dwSurrogateChar - 0x10000) % 0x400 + LOW_SURROGATE_START);
							}
							else
							{
								// Error : Buffer too small
								cchSrc++;
								break;
							}
						}

						cchWC += 2;
						bSurrogatePair = FALSE;
					}
				}
				else
				{
					//
					//  Make room for the trail byte and add the trail byte
					//  value.
					//
					if (cchDest)
					{
						lpDestStr[cchWC] <<= 6;
						lpDestStr[cchWC] |= LOWER_6_BIT(*pUTF8);
					}

					if (nTB == 0)
					{
						//
						//  End of sequence.  Advance the output counter.
						//
						cchWC++;
					}
				}
			}
			else
			{
				// error - not expecting a trail byte
				bSurrogatePair = FALSE;
			}
		}
		else
		{
			//
			//  Found a lead byte.
			//
			if (nTB > 0)
			{
				//
				//  Error - previous sequence not finished.
				//
				nTB = 0;
				bSurrogatePair = FALSE;
				cchWC++;
			}
			else
			{
				//
				//  Calculate the number of bytes to follow.
				//  Look for the first 0 from left to right.
				//
				UTF8 = *pUTF8;
				while (BIT7(UTF8) != 0)
				{
					UTF8 <<= 1;
					nTB++;
				}

				//
				// If this is a surrogate unicode pair
				//
				if (nTB == 4)
				{
					dwSurrogateChar = UTF8 >> nTB;
					bSurrogatePair = TRUE;
				}

				//
				//  Store the value from the first byte and decrement
				//  the number of bytes to follow.
				//
				if (cchDest)
				{
					lpDestStr[cchWC] = UTF8 >> nTB;
				}
				nTB--;
			}
		}

		pUTF8++;
	}

	//
	//  Make sure the destination buffer was large enough.
	//
	if (cchDest && (cchSrc >= 0))
	{
		(ERROR_INSUFFICIENT_BUFFER);
		return (0);
	}

	//
	//  Return the number of Unicode characters written.
	//
	return (cchWC);
}


////////////////////////////////////////////////////////////////////////////
//
//  UTFToUnicode
//
//  Maps a UTF character string to its wide character string counterpart.
//
//  02-06-96    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int UTFToUnicode(
	UINT CodePage,
	DWORD dwFlags,
	LPCSTR lpMultiByteStr,
	int cbMultiByte,
	LPWSTR lpWideCharStr,
	int cchWideChar)
{
	int rc = 0;


	//
	//  Invalid Parameter Check:
	//     - validate code page
	//     - length of MB string is 0
	//     - wide char buffer size is negative
	//     - MB string is NULL
	//     - length of WC string is NOT zero AND
	//         (WC string is NULL OR src and dest pointers equal)
	//
	if ((CodePage < CP_UTF7) || (CodePage > CP_UTF8) ||
		(cbMultiByte == 0) || (cchWideChar < 0) ||
		(lpMultiByteStr == NULL) ||
		((cchWideChar != 0) &&
		((lpWideCharStr == NULL) ||
			(lpMultiByteStr == (LPSTR)lpWideCharStr))))
	{
		(ERROR_INVALID_PARAMETER);
		return (0);
	}

	//
	//  Invalid Flags Check:
	//     - flags not 0
	//
	if (dwFlags != 0)
	{
		(ERROR_INVALID_FLAGS);
		return (0);
	}

	//
	//  If cbMultiByte is -1, then the string is null terminated and we
	//  need to get the length of the string.  Add one to the length to
	//  include the null termination.  (This will always be at least 1.)
	//
	if (cbMultiByte <= -1)
	{
		cbMultiByte = strlen(lpMultiByteStr) + 1;
	}

	switch (CodePage)
	{
	case (CP_UTF7):
	{
		rc = UTF7ToUnicode(lpMultiByteStr,
			cbMultiByte,
			lpWideCharStr,
			cchWideChar);
		break;
	}
	case (CP_UTF8):
	{
		rc = UTF8ToUnicode(lpMultiByteStr,
			cbMultiByte,
			lpWideCharStr,
			cchWideChar);
		break;
	}
	}

	return (rc);
}



////////////////////////////////////////////////////////////////////////////
//
//  GetCompositeChars
//
//  Gets the composite characters of a given wide character.  If the
//  composite form is found, it returns TRUE.  Otherwise, it returns
//  FALSE.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

BOOL  GetCompositeChars(
	WCHAR wch,
	WCHAR *pNonSp,
	WCHAR *pBase)
{
	PPRECOMP pPreComp;            // ptr to precomposed information


	//
	//  Store the ptr to the precomposed information.  No need to check if
	//  it's a NULL pointer since all tables in the Unicode file are
	//  constructed during initialization.
	//
	pPreComp = pTblPtrs->pPreComposed;

	//
	//  Traverse 8:4:4 table for base and nonspace character translation.
	//
	TRAVERSE_844_D(pPreComp, wch, *pNonSp, *pBase);

	//
	//  Return success if found.  Otherwise, error.
	//
	return ((*pNonSp) && (*pBase));
}



////////////////////////////////////////////////////////////////////////////
//
//  InsertCompositeForm
//
//  Gets the composite form of a given wide character, places it in the
//  wide character string, and returns the number of characters written.
//  If there is no composite form for the given character, the wide character
//  string is not touched.  It will return 1 for the number of characters
//  written, since the base character was already written.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int  InsertCompositeForm(
	LPWSTR pWCStr,
	LPWSTR pEndWCStr)
{
	WCHAR Base;                   // base character
	WCHAR NonSp;                  // non space character
	int wcCount = 0;              // number of wide characters written
	LPWSTR pEndComp;              // ptr to end of composite form
	int ctr;                      // loop counter


	//
	//  If no composite form can be found, return 1 for the base
	//  character that was already written.
	//
	if (!GetCompositeChars(*pWCStr, &NonSp, &Base))
	{
		return (1);
	}

	//
	//  Get the composite characters and write them to the pWCStr
	//  buffer.  Must check for multiple breakdowns of the precomposed
	//  character into more than 2 characters (multiple nonspacing
	//  characters).
	//
	pEndComp = pWCStr;
	do
	{
		//
		//  Make sure pWCStr is big enough to hold the nonspacing
		//  character.
		//
		if (pEndComp < (pEndWCStr - 1))
		{
			//
			//  Addition of next breakdown of nonspacing characters
			//  are to be added right after the base character.  So,
			//  move all nonspacing characters ahead one position
			//  to make room for the next nonspacing character.
			//
			pEndComp++;
			for (ctr = 0; ctr < wcCount; ctr++)
			{
				*(pEndComp - ctr) = *(pEndComp - (ctr + 1));
			}

			//
			//  Fill in the new base form and the new nonspacing character.
			//
			*pWCStr = Base;
			*(pWCStr + 1) = NonSp;
			wcCount++;
		}
		else
		{
			//
			//  Make sure we don't get into an infinite loop if the
			//  destination buffer isn't large enough.
			//
			break;
		}
	} while (GetCompositeChars(*pWCStr, &NonSp, &Base));

	//
	//  Return number of wide characters written.  Add 1 to include the
	//  base character.
	//
	return (wcCount + 1);
}

////////////////////////////////////////////////////////////////////////////
//
//  GetWCCompSBErr
//
//  Fills in pWCStr with the wide character(s) for the corresponding single
//  byte character from the appropriate translation table and returns the
//  number of wide characters written.  This routine should only be called
//  when the precomposed forms need to be translated to composite.
//
//  Checks to be sure an invalid character is not translated to the default
//  character.  If so, it sets last error and returns 0 characters written.
//
//  09-01-93    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetWCCompSBErr(
	PCP_HASH pHashN,
	PMB_TABLE pMBTbl,
	LPBYTE pMBStr,
	LPWSTR pWCStr,
	LPWSTR pEndWCStr)
{
	//
	//  Get the single byte to wide character translation.
	//
	GET_WC_SINGLE(pMBTbl, pMBStr, pWCStr);

	//
	//  Make sure an invalid character was not translated to the
	//  default char.  If it was, set last error and return 0
	//  characters written.
	//
	CHECK_ERROR_WC_SINGLE(pHashN, *pWCStr, *pMBStr);

	//
	//  Fill in the composite form of the character (if one exists)
	//  and return the number of wide characters written.
	//
	return (InsertCompositeForm(pWCStr, pEndWCStr));
}

////////////////////////////////////////////////////////////////////////////
//
//  GetWCCompSB
//
//  Fills in pWCStr with the wide character(s) for the corresponding single
//  byte character from the appropriate translation table and returns the
//  number of wide characters written.  This routine should only be called
//  when the precomposed forms need to be translated to composite.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetWCCompSB(
	PMB_TABLE pMBTbl,
	LPBYTE pMBStr,
	LPWSTR pWCStr,
	LPWSTR pEndWCStr)
{
	//
	//  Get the single byte to wide character translation.
	//
	GET_WC_SINGLE(pMBTbl, pMBStr, pWCStr);

	//
	//  Fill in the composite form of the character (if one exists)
	//  and return the number of wide characters written.
	//
	return (InsertCompositeForm(pWCStr, pEndWCStr));
}


////////////////////////////////////////////////////////////////////////////
//
//  GetWCCompMBErr
//
//  Fills in pWCStr with the wide character(s) for the corresponding multibyte
//  character from the appropriate translation table and returns the number
//  of wide characters written.  The number of bytes used from the pMBStr
//  buffer (single byte or double byte) is returned in the mbIncr parameter.
//  This routine should only be called when the precomposed forms need to be
//  translated to composite.
//
//  Checks to be sure an invalid character is not translated to the default
//  character.  If so, it sets last error and returns 0 characters written.
//
//  09-01-93    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetWCCompMBErr(
	PCP_HASH pHashN,
	PMB_TABLE pMBTbl,
	LPBYTE pMBStr,
	LPBYTE pEndMBStr,
	LPWSTR pWCStr,
	LPWSTR pEndWCStr,
	int *pmbIncr)
{
	//
	//  Get the multibyte to wide char translation.
	//
	//  Make sure an invalid character was not translated to the
	//  default char.  If it was, set last error and return 0
	//  characters written.
	//
	GET_WC_MULTI_ERR(pHashN,
		pMBTbl,
		pMBStr,
		pEndMBStr,
		pWCStr,
		pEndWCStr,
		*pmbIncr);

	//
	//  Fill in the composite form of the character (if one exists)
	//  and return the number of wide characters written.
	//
	return (InsertCompositeForm(pWCStr, pEndWCStr));
}


////////////////////////////////////////////////////////////////////////////
//
//  GetWCCompMB
//
//  Fills in pWCStr with the wide character(s) for the corresponding multibyte
//  character from the appropriate translation table and returns the number
//  of wide characters written.  The number of bytes used from the pMBStr
//  buffer (single byte or double byte) is returned in the mbIncr parameter.
//  This routine should only be called when the precomposed forms need to be
//  translated to composite.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

int GetWCCompMB(
	PCP_HASH pHashN,
	PMB_TABLE pMBTbl,
	LPBYTE pMBStr,
	LPBYTE pEndMBStr,
	LPWSTR pWCStr,
	LPWSTR pEndWCStr,
	int *pmbIncr)
{
	//
	//  Get the multibyte to wide char translation.
	//
	GET_WC_MULTI(pHashN,
		pMBTbl,
		pMBStr,
		pEndMBStr,
		pWCStr,
		pEndWCStr,
		*pmbIncr);

	//
	//  Fill in the composite form of the character (if one exists)
	//  and return the number of wide characters written.
	//
	return (InsertCompositeForm(pWCStr, pEndWCStr));
}



int __stdcall impl_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
{
	PCP_HASH pHashN;              // ptr to CP hash node
	register LPBYTE pMBStr;       // ptr to search through MB string
	register LPWSTR pWCStr;       // ptr to search through WC string
	LPBYTE pEndMBStr;             // ptr to end of MB search string
	LPWSTR pEndWCStr;             // ptr to end of WC string buffer
	int wcIncr;                   // amount to increment pWCStr
	int mbIncr;                   // amount to increment pMBStr
	int wcCount = 0;              // count of wide chars written
	int CompSet;                  // if MB_COMPOSITE flag is set
	PMB_TABLE pMBTbl;             // ptr to correct MB table (MB or GLYPH)
	int ctr;                      // loop counter


	//
	//  See if it's a special code page value for UTF translations.
	//
	if (CodePage >= NLS_CP_ALGORITHM_RANGE)
	{
		return (UTFToUnicode(CodePage,
			dwFlags,
			lpMultiByteStr,
			cbMultiByte,
			lpWideCharStr,
			cchWideChar));
	}

	//
	//  Get the code page value and the appropriate hash node.
	//
	GET_CP_HASH_NODE(CodePage, pHashN);

	//
	//  Invalid Parameter Check:
	//     - length of MB string is 0
	//     - wide char buffer size is negative
	//     - MB string is NULL
	//     - length of WC string is NOT zero AND
	//         (WC string is NULL OR src and dest pointers equal)
	//
	if ((cbMultiByte == 0) || (cchWideChar < 0) ||
		(lpMultiByteStr == NULL) ||
		((cchWideChar != 0) &&
		((lpWideCharStr == NULL) ||
			(lpMultiByteStr == (LPSTR)lpWideCharStr))))
	{
		(ERROR_INVALID_PARAMETER);
		return (0);
	}

	//
	//  If cbMultiByte is -1, then the string is null terminated and we
	//  need to get the length of the string.  Add one to the length to
	//  include the null termination.  (This will always be at least 1.)
	//
	if (cbMultiByte <= -1)
	{
		cbMultiByte = strlen(lpMultiByteStr) + 1;
	}

	//
	//  Check for valid code page.
	//
	if (pHashN == NULL)
	{
		//
		//  Special case the CP_SYMBOL code page.
		//
		if ((CodePage == CP_SYMBOL) && (dwFlags == 0))
		{
			//
			//  If the caller just wants the size of the buffer needed
			//  to do this translation, return the size of the MB string.
			//
			if (cchWideChar == 0)
			{
				return (cbMultiByte);
			}

			//
			//  Make sure the buffer is large enough.
			//
			if (cchWideChar < cbMultiByte)
			{
				(ERROR_INSUFFICIENT_BUFFER);
				return (0);
			}

			//
			//  Translate SB char xx to Unicode f0xx.
			//    0x00->0x1f map to 0x0000->0x001f
			//    0x20->0xff map to 0xf020->0xf0ff
			//
			for (ctr = 0; ctr < cbMultiByte; ctr++)
			{
				lpWideCharStr[ctr] = ((BYTE)(lpMultiByteStr[ctr]) < 0x20)
					? (WCHAR)lpMultiByteStr[ctr]
					: MAKEWORD(lpMultiByteStr[ctr], 0xf0);
			}
			return (cbMultiByte);
		}
		else
		{
			(((CodePage == CP_SYMBOL) && (dwFlags != 0))
				? ERROR_INVALID_FLAGS
				: ERROR_INVALID_PARAMETER);
			return (0);
		}
	}

	//
	//  See if the given code page is in the DLL range.
	//
	if (pHashN->pfnCPProc)
	{
		//
		//  Invalid Flags Check:
		//     - flags not 0
		//
		if (dwFlags != 0)
		{
			(ERROR_INVALID_FLAGS);
			return (0);
		}

		//
		//  Call the DLL to do the translation.
		//
		return ((*(pHashN->pfnCPProc))(CodePage,
			NLS_CP_MBTOWC,
			(LPSTR)lpMultiByteStr,
			cbMultiByte,
			(LPWSTR)lpWideCharStr,
			cchWideChar,
			NULL));
	}

	//
	//  Invalid Flags Check:
	//     - flags other than valid ones
	//     - composite and precomposed both set
	//
	if ((dwFlags & MB_INVALID_FLAG) ||
		((dwFlags & MB_PRECOMPOSED) && (dwFlags & MB_COMPOSITE)))
	{
		(ERROR_INVALID_FLAGS);
		return (0);
	}

	//
	//  Initialize multibyte character loop pointers.
	//
	pMBStr = (LPBYTE)lpMultiByteStr;
	pEndMBStr = pMBStr + cbMultiByte;
	CompSet = dwFlags & MB_COMPOSITE;

	//
	//  Get the correct MB table (MB or GLYPH).
	//
	if ((dwFlags & MB_USEGLYPHCHARS) && (pHashN->pGlyphTbl != NULL))
	{
		pMBTbl = pHashN->pGlyphTbl;
	}
	else
	{
		pMBTbl = pHashN->pMBTbl;
	}

	//
	//  If cchWideChar is 0, then we can't use lpWideCharStr.  In this
	//  case, we simply want to count the number of characters that would
	//  be written to the buffer.
	//
	if (cchWideChar == 0)
	{
		WCHAR pTempStr[MAX_COMPOSITE];   // tmp buffer - max for composite

		//
		//  For each multibyte char, translate it to its corresponding
		//  wide char and increment the wide character count.
		//
		pEndWCStr = pTempStr + MAX_COMPOSITE;
		if (IS_SBCS_CP(pHashN))
		{
			//
			//  Single Byte Character Code Page.
			//
			if (CompSet)
			{
				//
				//  Composite flag is set.
				//
				if (dwFlags & MB_ERR_INVALID_CHARS)
				{
					//
					//  Error check flag is set.
					//
					while (pMBStr < pEndMBStr)
					{
						if (!(wcIncr = GetWCCompSBErr(pHashN,
							pMBTbl,
							pMBStr,
							pTempStr,
							pEndWCStr)))
						{
							return (0);
						}
						pMBStr++;
						wcCount += wcIncr;
					}
				}
				else
				{
					//
					//  Error check flag is NOT set.
					//
					while (pMBStr < pEndMBStr)
					{
						wcCount += GetWCCompSB(pMBTbl,
							pMBStr,
							pTempStr,
							pEndWCStr);
						pMBStr++;
					}
				}
			}
			else
			{
				//
				//  Composite flag is NOT set.
				//
				if (dwFlags & MB_ERR_INVALID_CHARS)
				{
					//
					//  Error check flag is set.
					//
					wcCount = (int)(pEndMBStr - pMBStr);
					while (pMBStr < pEndMBStr)
					{
						GET_WC_SINGLE(pMBTbl,
							pMBStr,
							pTempStr);
						CHECK_ERROR_WC_SINGLE(pHashN,
							*pTempStr,
							*pMBStr);
						pMBStr++;
					}
				}
				else
				{
					//
					//  Error check flag is NOT set.
					//
					//  Just return the size of the MB string, since
					//  it's a 1:1 translation.
					//
					wcCount = (int)(pEndMBStr - pMBStr);
				}
			}
		}
		else
		{
			//
			//  Multi Byte Character Code Page.
			//
			if (CompSet)
			{
				//
				//  Composite flag is set.
				//
				if (dwFlags & MB_ERR_INVALID_CHARS)
				{
					//
					//  Error check flag is set.
					//
					while (pMBStr < pEndMBStr)
					{
						if (!(wcIncr = GetWCCompMBErr(pHashN,
							pMBTbl,
							pMBStr,
							pEndMBStr,
							pTempStr,
							pEndWCStr,
							&mbIncr)))
						{
							return (0);
						}
						pMBStr += mbIncr;
						wcCount += wcIncr;
					}
				}
				else
				{
					//
					//  Error check flag is NOT set.
					//
					while (pMBStr < pEndMBStr)
					{
						wcCount += GetWCCompMB(pHashN,
							pMBTbl,
							pMBStr,
							pEndMBStr,
							pTempStr,
							pEndWCStr,
							&mbIncr);
						pMBStr += mbIncr;
					}
				}
			}
			else
			{
				//
				//  Composite flag is NOT set.
				//
				if (dwFlags & MB_ERR_INVALID_CHARS)
				{
					//
					//  Error check flag is set.
					//
					while (pMBStr < pEndMBStr)
					{
						GET_WC_MULTI_ERR(pHashN,
							pMBTbl,
							pMBStr,
							pEndMBStr,
							pTempStr,
							pEndWCStr,
							mbIncr);
						pMBStr += mbIncr;
						wcCount++;
					}
				}
				else
				{
					//
					//  Error check flag is NOT set.
					//
					while (pMBStr < pEndMBStr)
					{
						GET_WC_MULTI(pHashN,
							pMBTbl,
							pMBStr,
							pEndMBStr,
							pTempStr,
							pEndWCStr,
							mbIncr);
						pMBStr += mbIncr;
						wcCount++;
					}
				}
			}
		}
	}
	else
	{
		//
		//  Initialize wide character loop pointers.
		//
		pWCStr = lpWideCharStr;
		pEndWCStr = pWCStr + cchWideChar;

		//
		//  For each multibyte char, translate it to its corresponding
		//  wide char, store it in lpWideCharStr, and increment the wide
		//  character count.
		//
		if (IS_SBCS_CP(pHashN))
		{
			//
			//  Single Byte Character Code Page.
			//
			if (CompSet)
			{
				//
				//  Composite flag is set.
				//
				if (dwFlags & MB_ERR_INVALID_CHARS)
				{
					//
					//  Error check flag is set.
					//
					while ((pMBStr < pEndMBStr) && (pWCStr < pEndWCStr))
					{
						if (!(wcIncr = GetWCCompSBErr(pHashN,
							pMBTbl,
							pMBStr,
							pWCStr,
							pEndWCStr)))
						{
							return (0);
						}
						pMBStr++;
						pWCStr += wcIncr;
					}
					wcCount = (int)(pWCStr - lpWideCharStr);
				}
				else
				{
					//
					//  Error check flag is NOT set.
					//
					while ((pMBStr < pEndMBStr) && (pWCStr < pEndWCStr))
					{
						pWCStr += GetWCCompSB(pMBTbl,
							pMBStr,
							pWCStr,
							pEndWCStr);
						pMBStr++;
					}
					wcCount = (int)(pWCStr - lpWideCharStr);
				}
			}
			else
			{
				//
				//  Composite flag is NOT set.
				//
				if (dwFlags & MB_ERR_INVALID_CHARS)
				{
					//
					//  Error check flag is set.
					//
					wcCount = (int)(pEndMBStr - pMBStr);
					if ((pEndWCStr - pWCStr) < wcCount)
					{
						wcCount = (int)(pEndWCStr - pWCStr);
					}
					for (ctr = wcCount; ctr > 0; ctr--)
					{
						GET_WC_SINGLE(pMBTbl,
							pMBStr,
							pWCStr);
						CHECK_ERROR_WC_SINGLE(pHashN,
							*pWCStr,
							*pMBStr);
						pMBStr++;
						pWCStr++;
					}
				}
				else
				{
					//
					//  Error check flag is NOT set.
					//
					wcCount = (int)(pEndMBStr - pMBStr);
					if ((pEndWCStr - pWCStr) < wcCount)
					{
						wcCount = (int)(pEndWCStr - pWCStr);
					}
					for (ctr = wcCount; ctr > 0; ctr--)
					{
						GET_WC_SINGLE(pMBTbl,
							pMBStr,
							pWCStr);
						pMBStr++;
						pWCStr++;
					}
				}
			}
		}
		else
		{
			//
			//  Multi Byte Character Code Page.
			//
			if (CompSet)
			{
				//
				//  Composite flag is set.
				//
				if (dwFlags & MB_ERR_INVALID_CHARS)
				{
					//
					//  Error check flag is set.
					//
					while ((pMBStr < pEndMBStr) && (pWCStr < pEndWCStr))
					{
						if (!(wcIncr = GetWCCompMBErr(pHashN,
							pMBTbl,
							pMBStr,
							pEndMBStr,
							pWCStr,
							pEndWCStr,
							&mbIncr)))
						{
							return (0);
						}
						pMBStr += mbIncr;
						pWCStr += wcIncr;
					}
					wcCount = (int)(pWCStr - lpWideCharStr);
				}
				else
				{
					//
					//  Error check flag is NOT set.
					//
					while ((pMBStr < pEndMBStr) && (pWCStr < pEndWCStr))
					{
						pWCStr += GetWCCompMB(pHashN,
							pMBTbl,
							pMBStr,
							pEndMBStr,
							pWCStr,
							pEndWCStr,
							&mbIncr);
						pMBStr += mbIncr;
					}
					wcCount = (int)(pWCStr - lpWideCharStr);
				}
			}
			else
			{
				//
				//  Composite flag is NOT set.
				//
				if (dwFlags & MB_ERR_INVALID_CHARS)
				{
					//
					//  Error check flag is set.
					//
					while ((pMBStr < pEndMBStr) && (pWCStr < pEndWCStr))
					{
						GET_WC_MULTI_ERR(pHashN,
							pMBTbl,
							pMBStr,
							pEndMBStr,
							pWCStr,
							pEndWCStr,
							mbIncr);
						pMBStr += mbIncr;
						pWCStr++;
					}
					wcCount = (int)(pWCStr - lpWideCharStr);
				}
				else
				{
					//
					//  Error check flag is NOT set.
					//
					while ((pMBStr < pEndMBStr) && (pWCStr < pEndWCStr))
					{
						GET_WC_MULTI(pHashN,
							pMBTbl,
							pMBStr,
							pEndMBStr,
							pWCStr,
							pEndWCStr,
							mbIncr);
						pMBStr += mbIncr;
						pWCStr++;
					}
					wcCount = (int)(pWCStr - lpWideCharStr);
				}
			}
		}

		//
		//  Make sure wide character buffer was large enough.
		//
		if (pMBStr < pEndMBStr)
		{
			(ERROR_INSUFFICIENT_BUFFER);
			return (0);
		}
	}

	//
	//  Return the number of characters written (or that would have
	//  been written) to the buffer.
	//
	return (wcCount);
}